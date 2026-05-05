#!/usr/bin/env python3

from __future__ import annotations

import csv
import subprocess
import tempfile
import unittest
from pathlib import Path


class SummarizeThesisResultsTestCase(unittest.TestCase):
    def _write_run(self, run_dir: Path, delay_ms: float, throughput_mbps: float) -> None:
        run_dir.mkdir(parents=True)
        (run_dir / "e2e_flow_metrics.csv").write_text(
            "ue,dl_port,matched_flow,tx_packets,rx_packets,lost_packets,"
            "loss_rate_percent,tx_bytes,rx_bytes,offered_mbps,throughput_mbps,"
            "mean_delay_ms,mean_jitter_ms\n"
            f"TOTAL,,,100,80,20,20.0,100000,80000,10.0,{throughput_mbps},{delay_ms},1.0\n",
            encoding="utf-8",
        )
        (run_dir / "handover_event_trace.csv").write_text(
            "time_s,ue,ho_id,event,source_cell,target_cell,source_sat,target_sat,delay_ms,"
            "ping_pong_detected,failure_reason\n"
            "1.0,0,1,HO_END_OK,1,2,0,1,10.0,0,\n"
            "2.0,0,2,HO_END_OK,2,1,1,0,10.0,1,\n",
            encoding="utf-8",
        )
        (run_dir / "satellite_state_trace.csv").write_text(
            "time_s,sat,plane,slot,cell,anchor_grid_id,attached_ue_count,"
            "offered_packet_rate,load_score,admission_allowed\n"
            "0.0,0,0,0,1,1,1,250,0.2,1\n"
            "0.0,1,0,1,2,2,3,750,0.6,1\n"
            "1.0,0,0,0,1,1,2,500,0.4,1\n",
            encoding="utf-8",
        )

    def test_writes_only_formal_summary_csvs(self) -> None:
        script_path = Path(__file__).with_name("summarize_thesis_results.py")

        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            results_root = tmp / "formal"
            self._write_run(results_root / "baseline" / "seed-01", 100.0, 10.0)
            self._write_run(results_root / "improved" / "seed-01", 90.0, 11.0)
            output_dir = tmp / "summary"

            subprocess.run(
                [
                    "python3",
                    str(script_path),
                    "--results-root",
                    str(results_root),
                    "--output-dir",
                    str(output_dir),
                ],
                check=True,
            )

            generated = {path.name for path in output_dir.iterdir()}
            self.assertEqual(
                generated,
                {"run_summary.csv", "paper_kpi_summary.csv", "paper_kpi_comparison.csv"},
            )

            with (output_dir / "run_summary.csv").open(newline="", encoding="utf-8") as handle:
                rows = list(csv.DictReader(handle))
            self.assertEqual(len(rows), 2)
            self.assertEqual(rows[0]["run_id"], "baseline/seed-01")
            self.assertEqual(rows[1]["run_id"], "improved/seed-01")

            with (output_dir / "paper_kpi_comparison.csv").open(
                newline="", encoding="utf-8"
            ) as handle:
                comparison_rows = {row["metric"]: row for row in csv.DictReader(handle)}
            self.assertEqual(comparison_rows["e2e_delay_ms"]["better_direction"], "lower")
            self.assertEqual(comparison_rows["throughput_mbps"]["better_direction"], "higher")


if __name__ == "__main__":
    unittest.main(verbosity=2)

#!/usr/bin/env python3
"""Summarize formal thesis runs for baseline-vs-improved comparison."""

from __future__ import annotations

import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple


RUN_SUMMARY_FIELDS = [
    "run_id",
    "mode",
    "seed",
    "e2e_delay_ms",
    "packet_loss_percent",
    "throughput_mbps",
    "completed_ho",
    "ping_pong_count",
    "load_balance_jain",
]

KPI_FIELDS = [
    "e2e_delay_ms",
    "packet_loss_percent",
    "throughput_mbps",
    "completed_ho",
    "ping_pong_count",
    "load_balance_jain",
]

SUMMARY_FIELDS = ["mode", "runs"] + [
    f"{field}_{suffix}" for field in KPI_FIELDS for suffix in ("mean", "std")
]

COMPARISON_FIELDS = [
    "metric",
    "baseline_mean",
    "baseline_std",
    "improved_mean",
    "improved_std",
    "absolute_delta",
    "relative_delta_percent",
    "better_direction",
]

BETTER_DIRECTIONS = {
    "e2e_delay_ms": "lower",
    "packet_loss_percent": "lower",
    "throughput_mbps": "higher",
    "completed_ho": "context",
    "ping_pong_count": "lower",
    "load_balance_jain": "higher",
}


def _clean_csv_lines(handle: Iterable[str]) -> Iterable[str]:
    for line in handle:
        yield line.replace("\0", "")


def _safe_float(value: Optional[str], default: float = math.nan) -> float:
    if value is None:
        return default
    text = value.strip()
    if not text:
        return default
    try:
        return float(text)
    except ValueError:
        return default


def _safe_int(value: Optional[str], default: int = 0) -> int:
    if value is None:
        return default
    text = value.strip()
    if not text:
        return default
    try:
        return int(text)
    except ValueError:
        return default


def _read_csv(path: Path) -> Tuple[List[Dict[str, str]], List[str]]:
    if not path.exists():
        raise FileNotFoundError(f"missing required file: {path}")
    with path.open("r", newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(_clean_csv_lines(handle))
        if not reader.fieldnames:
            raise RuntimeError(f"empty or invalid csv: {path}")
        return list(reader), list(reader.fieldnames)


def _require_columns(path: Path, fieldnames: Iterable[str], required: Iterable[str]) -> None:
    columns = set(fieldnames)
    missing = set(required).difference(columns)
    if missing:
        raise RuntimeError(f"{path} missing required columns: {sorted(missing)}")


def _read_e2e_metrics(run_dir: Path) -> Dict[str, float]:
    path = run_dir / "e2e_flow_metrics.csv"
    rows, fieldnames = _read_csv(path)
    _require_columns(
        path,
        fieldnames,
        ["ue", "loss_rate_percent", "throughput_mbps", "mean_delay_ms"],
    )
    total = next((row for row in rows if row.get("ue", "").strip() == "TOTAL"), None)
    if total is None:
        raise RuntimeError(f"{path} has no TOTAL row")
    return {
        "e2e_delay_ms": _safe_float(total.get("mean_delay_ms")),
        "packet_loss_percent": _safe_float(total.get("loss_rate_percent")),
        "throughput_mbps": _safe_float(total.get("throughput_mbps")),
    }


def _read_handover_metrics(run_dir: Path) -> Dict[str, int]:
    path = run_dir / "handover_event_trace.csv"
    rows, fieldnames = _read_csv(path)
    _require_columns(path, fieldnames, ["event", "ping_pong_detected"])
    completed = sum(1 for row in rows if row.get("event", "").strip() == "HO_END_OK")
    ping_pong = sum(1 for row in rows if _safe_int(row.get("ping_pong_detected")) == 1)
    return {"completed_ho": completed, "ping_pong_count": ping_pong}


def _jain_index(values: List[float]) -> float:
    total = sum(values)
    square_sum = sum(value * value for value in values)
    if total <= 0.0 or square_sum <= 0.0:
        return math.nan
    return (total * total) / (len(values) * square_sum)


def _read_satellite_state_metrics(run_dir: Path) -> Dict[str, float]:
    path = run_dir / "satellite_state_trace.csv"
    rows, fieldnames = _read_csv(path)
    _require_columns(path, fieldnames, ["time_s", "sat", "attached_ue_count"])

    by_time: Dict[str, Dict[str, float]] = defaultdict(dict)
    sat_keys: set[str] = set()
    for row in rows:
        time_key = row.get("time_s", "").strip()
        sat_key = row.get("sat", "").strip()
        if not time_key or not sat_key:
            continue
        sat_keys.add(sat_key)
        by_time[time_key][sat_key] = _safe_float(row.get("attached_ue_count"), 0.0)

    jain_values: List[float] = []
    for sat_values in by_time.values():
        values = [sat_values.get(sat_key, 0.0) for sat_key in sorted(sat_keys)]
        value = _jain_index(values)
        if math.isfinite(value):
            jain_values.append(value)

    if not jain_values:
        raise RuntimeError(f"{path} has no non-empty satellite load samples")
    return {"load_balance_jain": statistics.fmean(jain_values)}


def _discover_runs(results_root: Path) -> List[Dict[str, object]]:
    runs: List[Dict[str, object]] = []
    seed_sets: Dict[str, set[str]] = {}
    for mode in ("baseline", "improved"):
        mode_dir = results_root / mode
        if not mode_dir.exists():
            seed_sets[mode] = set()
            continue
        run_dirs = sorted(
            path for path in mode_dir.iterdir() if path.is_dir() and path.name.startswith("seed-")
        )
        seed_sets[mode] = {path.name for path in run_dirs}
        for run_dir in run_dirs:
            runs.append(
                {
                    "mode": mode,
                    "seed": run_dir.name,
                    "run_id": f"{mode}/{run_dir.name}",
                    "path": run_dir,
                }
            )
    missing_modes = [mode for mode in ("baseline", "improved") if not seed_sets.get(mode)]
    if missing_modes:
        raise RuntimeError(
            f"missing formal run directories for modes {missing_modes} under {results_root}"
        )
    if seed_sets["baseline"] != seed_sets["improved"]:
        raise RuntimeError(
            "baseline and improved formal runs must use the same seed directories: "
            f"baseline={sorted(seed_sets['baseline'])}, improved={sorted(seed_sets['improved'])}"
        )
    return runs


def _summarize_run(run: Dict[str, object]) -> Dict[str, object]:
    run_dir = Path(run["path"])
    summary: Dict[str, object] = {
        "run_id": run["run_id"],
        "mode": run["mode"],
        "seed": run["seed"],
    }
    summary.update(_read_e2e_metrics(run_dir))
    summary.update(_read_handover_metrics(run_dir))
    summary.update(_read_satellite_state_metrics(run_dir))
    return summary


def _mean(values: List[float]) -> float:
    finite = [value for value in values if math.isfinite(value)]
    return statistics.fmean(finite) if finite else math.nan


def _std(values: List[float]) -> float:
    finite = [value for value in values if math.isfinite(value)]
    if len(finite) < 2:
        return 0.0 if finite else math.nan
    return statistics.stdev(finite)


def _summarize_modes(run_summaries: List[Dict[str, object]]) -> List[Dict[str, object]]:
    by_mode: Dict[str, List[Dict[str, object]]] = defaultdict(list)
    for summary in run_summaries:
        by_mode[str(summary["mode"])].append(summary)

    mode_rows: List[Dict[str, object]] = []
    for mode in ("baseline", "improved"):
        rows = by_mode.get(mode, [])
        if not rows:
            continue
        out: Dict[str, object] = {"mode": mode, "runs": len(rows)}
        for field in KPI_FIELDS:
            values = [float(row[field]) for row in rows]
            out[f"{field}_mean"] = _mean(values)
            out[f"{field}_std"] = _std(values)
        mode_rows.append(out)
    return mode_rows


def _comparison_rows(mode_rows: List[Dict[str, object]]) -> List[Dict[str, object]]:
    by_mode = {str(row["mode"]): row for row in mode_rows}
    if "baseline" not in by_mode or "improved" not in by_mode:
        return []
    baseline = by_mode["baseline"]
    improved = by_mode["improved"]
    rows: List[Dict[str, object]] = []
    for metric in KPI_FIELDS:
        base_mean = float(baseline[f"{metric}_mean"])
        imp_mean = float(improved[f"{metric}_mean"])
        delta = imp_mean - base_mean
        relative = math.nan
        if base_mean != 0.0 and math.isfinite(base_mean):
            relative = 100.0 * delta / abs(base_mean)
        rows.append(
            {
                "metric": metric,
                "baseline_mean": base_mean,
                "baseline_std": baseline[f"{metric}_std"],
                "improved_mean": imp_mean,
                "improved_std": improved[f"{metric}_std"],
                "absolute_delta": delta,
                "relative_delta_percent": relative,
                "better_direction": BETTER_DIRECTIONS[metric],
            }
        )
    return rows


def _format_value(value: object) -> object:
    if isinstance(value, float):
        if math.isfinite(value):
            return f"{value:.6f}"
        return ""
    return value


def _write_rows(path: Path, fieldnames: List[str], rows: List[Dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: _format_value(row.get(field, "")) for field in fieldnames})


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--results-root",
        type=Path,
        default=Path("scratch/results/formal/v6.1-poisson3ring-overlap-beamonly-40s"),
        help="Formal results root containing baseline/ and improved/ run directories.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory for summary CSV outputs. Defaults to <results-root>/summary.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    results_root = args.results_root
    output_dir = args.output_dir or (results_root / "summary")

    runs = _discover_runs(results_root)
    run_summaries = [_summarize_run(run) for run in runs]
    mode_rows = _summarize_modes(run_summaries)
    comparisons = _comparison_rows(mode_rows)

    _write_rows(output_dir / "run_summary.csv", RUN_SUMMARY_FIELDS, run_summaries)
    _write_rows(output_dir / "paper_kpi_summary.csv", SUMMARY_FIELDS, mode_rows)
    _write_rows(output_dir / "paper_kpi_comparison.csv", COMPARISON_FIELDS, comparisons)

    print(f"[OK] wrote thesis summaries to {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

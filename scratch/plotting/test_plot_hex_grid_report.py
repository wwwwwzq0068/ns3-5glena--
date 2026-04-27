#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


class PlotHexGridReportTestCase(unittest.TestCase):
    def test_generates_html_report_with_ground_and_anchor_tracks(self) -> None:
        script_path = Path(__file__).with_name("plot_hex_grid_svg.py")

        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            grid_csv = tmp / "hex_grid_cells.csv"
            anchor_csv = tmp / "sat_anchor_trace.csv"
            ground_csv = tmp / "sat_ground_track.csv"
            ue_csv = tmp / "ue_layout.csv"
            html_out = tmp / "hex_grid_cells.html"

            grid_csv.write_text(
                "id,latitude_deg,longitude_deg,altitude_m,east_m,north_m,ecef_x,ecef_y,ecef_z\n"
                "1,45.0,84.0,0,0,0,0,0,0\n"
                "2,45.0,84.1,0,1732.051,0,0,0,0\n",
                encoding="utf-8",
            )
            anchor_csv.write_text(
                "time_s,sat,plane,slot,cell,anchor_grid_id,anchor_latitude_deg,anchor_longitude_deg,anchor_east_m,anchor_north_m\n"
                "0.0,0,0,0,1,1,45.0,84.0,0,0\n"
                "1.0,0,0,0,1,2,45.0,84.1,1732.051,0\n",
                encoding="utf-8",
            )
            ground_csv.write_text(
                "time_s,sat,plane,slot,cell,subpoint_latitude_deg,subpoint_longitude_deg,subpoint_east_m,subpoint_north_m,sat_ecef_x,sat_ecef_y,sat_ecef_z\n"
                "0.0,0,0,0,1,45.0,84.0,0,0,1,2,3\n"
                "1.0,0,0,0,1,45.0,84.1,1732.051,0,4,5,6\n",
                encoding="utf-8",
            )
            ue_csv.write_text(
                "ue_id,role,east_m,north_m,latitude_deg,longitude_deg,altitude_m\n"
                "1,center,100,50,45.0,84.0,0\n"
                "2,ring,900,100,45.0,84.1,0\n",
                encoding="utf-8",
            )

            subprocess.run(
                [
                    "python3",
                    str(script_path),
                    "--csv",
                    str(grid_csv),
                    "--sat-anchor-csv",
                    str(anchor_csv),
                    "--sat-ground-track-csv",
                    str(ground_csv),
                    "--html-out",
                    str(html_out),
                ],
                check=True,
            )

            html = html_out.read_text(encoding="utf-8")
            self.assertIn("Show all", html)
            self.assertIn("Clear all", html)
            self.assertIn("Real track", html)
            self.assertIn("Beam landing track", html)
            self.assertIn("sat0", html)
            self.assertIn("groundTrackData", html)
            self.assertIn("anchorTrackData", html)
            self.assertIn("UE distribution", html)
            self.assertIn("UE1", html)

    def test_applies_plane_specific_north_offsets_to_tracks_only(self) -> None:
        script_path = Path(__file__).with_name("plot_hex_grid_svg.py")

        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            grid_csv = tmp / "hex_grid_cells.csv"
            anchor_csv = tmp / "sat_anchor_trace.csv"
            ground_csv = tmp / "sat_ground_track.csv"
            html_out = tmp / "hex_grid_cells.html"

            grid_csv.write_text(
                "id,latitude_deg,longitude_deg,altitude_m,east_m,north_m,ecef_x,ecef_y,ecef_z\n"
                "1,45.0,84.0,0,0,0,0,0,0\n"
                "2,45.0,84.1,0,1732.051,0,0,0,0\n",
                encoding="utf-8",
            )
            anchor_csv.write_text(
                "time_s,sat,plane,slot,cell,anchor_grid_id,anchor_latitude_deg,anchor_longitude_deg,anchor_east_m,anchor_north_m\n"
                "0.0,0,0,0,1,1,45.0,84.0,0,0\n"
                "0.0,1,1,0,2,2,45.0,84.1,1732.051,0\n",
                encoding="utf-8",
            )
            ground_csv.write_text(
                "time_s,sat,plane,slot,cell,subpoint_latitude_deg,subpoint_longitude_deg,subpoint_east_m,subpoint_north_m,sat_ecef_x,sat_ecef_y,sat_ecef_z\n"
                "0.0,0,0,0,1,45.0,84.0,0,0,1,2,3\n"
                "0.0,1,1,0,2,45.0,84.1,1732.051,0,4,5,6\n",
                encoding="utf-8",
            )

            subprocess.run(
                [
                    "python3",
                    str(script_path),
                    "--csv",
                    str(grid_csv),
                    "--sat-anchor-csv",
                    str(anchor_csv),
                    "--sat-ground-track-csv",
                    str(ground_csv),
                    "--plane-north-offsets-m",
                    "0:-500",
                    "--html-out",
                    str(html_out),
                ],
                check=True,
            )

            html = html_out.read_text(encoding="utf-8")
            self.assertIn('"north_m":-500.0', html)
            self.assertIn('"north_m":0.0', html)
            self.assertIn('"anchor_grid_id":1', html)
            self.assertIn('"anchor_grid_id":2', html)


if __name__ == "__main__":
    unittest.main(verbosity=2)

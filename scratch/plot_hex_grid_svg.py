#!/usr/bin/env python3
"""
Plot WGS84 hex grid CSV into an SVG image without third-party dependencies.

Input CSV columns expected:
id, latitude_deg, longitude_deg, altitude_m, east_m, north_m, ecef_x, ecef_y, ecef_z
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from statistics import median
from typing import Dict, List, Optional, Tuple


def _parse_bool(value: str) -> bool:
    v = value.strip().lower()
    if v in {"1", "true", "yes", "y", "on"}:
        return True
    if v in {"0", "false", "no", "n", "off"}:
        return False
    raise argparse.ArgumentTypeError(f"invalid bool: {value}")


def load_cells(csv_path: Path) -> List[Dict[str, float]]:
    cells: List[Dict[str, float]] = []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        required = {"id", "east_m", "north_m", "latitude_deg", "longitude_deg"}
        missing = required.difference(reader.fieldnames or [])
        if missing:
            raise ValueError(f"missing required columns: {sorted(missing)}")
        for row in reader:
            cells.append(
                {
                    "id": int(row["id"]),
                    "east_m": float(row["east_m"]),
                    "north_m": float(row["north_m"]),
                    "lat_deg": float(row["latitude_deg"]),
                    "lon_deg": float(row["longitude_deg"]),
                }
            )
    if not cells:
        raise ValueError("CSV has no rows")
    return cells


def load_ues(csv_path: Path) -> List[Dict[str, float | str | int]]:
    ues: List[Dict[str, float | str | int]] = []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        required = {"ue_id", "role", "east_m", "north_m"}
        missing = required.difference(reader.fieldnames or [])
        if missing:
            raise ValueError(f"missing required UE columns: {sorted(missing)}")
        for row in reader:
            ues.append(
                {
                    "ue_id": int(row["ue_id"]),
                    "role": row["role"].strip(),
                    "east_m": float(row["east_m"]),
                    "north_m": float(row["north_m"]),
                }
            )
    return ues


def load_sat_anchor_trace(csv_path: Path) -> List[Dict[str, float | int]]:
    rows: List[Dict[str, float | int]] = []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        required = {
            "time_s",
            "sat",
            "plane",
            "slot",
            "cell",
            "anchor_grid_id",
            "anchor_east_m",
            "anchor_north_m",
        }
        missing = required.difference(reader.fieldnames or [])
        if missing:
            raise ValueError(f"missing required satellite anchor columns: {sorted(missing)}")
        for row in reader:
            anchor_east_m = float(row["anchor_east_m"])
            anchor_north_m = float(row["anchor_north_m"])
            if not math.isfinite(anchor_east_m) or not math.isfinite(anchor_north_m):
                continue
            rows.append(
                {
                    "time_s": float(row["time_s"]),
                    "sat": int(row["sat"]),
                    "plane": int(row["plane"]),
                    "slot": int(row["slot"]),
                    "cell": int(row["cell"]),
                    "anchor_grid_id": int(row["anchor_grid_id"]),
                    "anchor_east_m": anchor_east_m,
                    "anchor_north_m": anchor_north_m,
                }
            )
    return rows


def infer_hex_radius_m(cells: List[Dict[str, float]]) -> float:
    # For pointy-top hex grid, same-row center spacing is dx = sqrt(3) * r.
    row_to_east: Dict[float, List[float]] = {}
    for c in cells:
        key = round(c["north_m"], 3)
        row_to_east.setdefault(key, []).append(c["east_m"])

    row_diffs: List[float] = []
    for easts in row_to_east.values():
        if len(easts) < 2:
            continue
        easts = sorted(easts)
        for i in range(1, len(easts)):
            d = abs(easts[i] - easts[i - 1])
            if d > 1e-6:
                row_diffs.append(d)

    if not row_diffs:
        return 1.0
    dx = median(row_diffs)
    return dx / math.sqrt(3.0)


def hex_vertices(east: float, north: float, radius: float) -> List[Tuple[float, float]]:
    # Pointy-top hex
    pts: List[Tuple[float, float]] = []
    for k in range(6):
        ang = math.radians(60.0 * k - 30.0)
        pts.append((east + radius * math.cos(ang), north + radius * math.sin(ang)))
    return pts


def _dedupe_track_points(points: List[Tuple[float, float]]) -> List[Tuple[float, float]]:
    deduped: List[Tuple[float, float]] = []
    for point in points:
        if not deduped or point != deduped[-1]:
            deduped.append(point)
    return deduped


def render_svg(
    cells: List[Dict[str, float]],
    out_path: Path,
    width: int,
    height: int,
    margin: int,
    show_labels: bool,
    label_step: int,
    show_centers: bool,
    title: str,
    ue_points: Optional[List[Dict[str, float | str | int]]] = None,
    sat_anchor_points: Optional[List[Dict[str, float | int]]] = None,
    ue_label_prefix: str = "UE",
    ue_show_labels: bool = True,
    subtitle: str = "",
) -> None:
    east_vals = [c["east_m"] for c in cells]
    north_vals = [c["north_m"] for c in cells]
    if sat_anchor_points:
        east_vals.extend(float(p["anchor_east_m"]) for p in sat_anchor_points)
        north_vals.extend(float(p["anchor_north_m"]) for p in sat_anchor_points)
    min_e, max_e = min(east_vals), max(east_vals)
    min_n, max_n = min(north_vals), max(north_vals)
    span_e = max(max_e - min_e, 1.0)
    span_n = max(max_n - min_n, 1.0)

    radius_m = infer_hex_radius_m(cells)

    plot_w = max(1, width - 2 * margin)
    plot_h = max(1, height - 2 * margin)
    sx = plot_w / span_e
    sy = plot_h / span_n
    s = min(sx, sy)

    def map_xy(east: float, north: float) -> Tuple[float, float]:
        x = margin + (east - min_e) * s
        y = height - margin - (north - min_n) * s
        return x, y

    def map_poly(poly: List[Tuple[float, float]]) -> str:
        mapped = [map_xy(p[0], p[1]) for p in poly]
        return " ".join(f"{x:.2f},{y:.2f}" for x, y in mapped)

    lines: List[str] = []
    lines.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">'
    )
    lines.append('<rect x="0" y="0" width="100%" height="100%" fill="#ffffff"/>')
    lines.append(
        f'<text x="{margin}" y="{margin - 12}" font-size="16" fill="#111111" '
        f'font-family="monospace">{title}</text>'
    )
    if subtitle:
        lines.append(
            f'<text x="{margin}" y="{margin + 10}" font-size="12" fill="#444444" '
            f'font-family="monospace">{subtitle}</text>'
        )

    for i, c in enumerate(cells):
        poly = hex_vertices(c["east_m"], c["north_m"], radius_m)
        lines.append(
            f'<polygon points="{map_poly(poly)}" fill="#eaf3ff" stroke="#4f6d8a" stroke-width="1"/>'
        )
        cx, cy = map_xy(c["east_m"], c["north_m"])
        if show_centers:
            lines.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="1.8" fill="#1c1c1c"/>')
        if show_labels and (i % max(1, label_step) == 0):
            lines.append(
                f'<text x="{cx + 3:.2f}" y="{cy - 3:.2f}" font-size="9" fill="#0a1f44" '
                f'font-family="monospace">{int(c["id"])}</text>'
            )

    if sat_anchor_points:
        plane_colors = {
            0: "#d9480f",
            1: "#1c7ed6",
        }
        by_plane: Dict[int, Dict[int, List[Dict[str, float | int]]]] = {}
        by_sat: Dict[int, List[Dict[str, float | int]]] = {}
        sat_to_plane: Dict[int, int] = {}
        for row in sat_anchor_points:
            plane = int(row["plane"])
            sat = int(row["sat"])
            by_plane.setdefault(plane, {}).setdefault(sat, []).append(row)
            by_sat.setdefault(sat, []).append(row)
            sat_to_plane[sat] = plane

        for sat_rows in by_sat.values():
            sat_rows.sort(key=lambda item: float(item["time_s"]))
        for plane_rows in by_plane.values():
            for sat_rows in plane_rows.values():
                sat_rows.sort(key=lambda item: float(item["time_s"]))

        # 同轨卫星主要是时间错位，这里用每个轨道面 slot 最小的卫星作为连续主线代表。
        for plane, sat_rows_map in sorted(by_plane.items()):
            representative_sat = min(
                sat_rows_map,
                key=lambda sat: (int(sat_rows_map[sat][0]["slot"]), sat),
            )
            rep_rows = sat_rows_map[representative_sat]
            rep_points = _dedupe_track_points(
                [
                    (float(row["anchor_east_m"]), float(row["anchor_north_m"]))
                    for row in rep_rows
                ]
            )
            if len(rep_points) >= 2:
                mapped_points = " ".join(
                    f"{x:.2f},{y:.2f}" for x, y in (map_xy(east, north) for east, north in rep_points)
                )
                lines.append(
                    f'<polyline points="{mapped_points}" fill="none" '
                    f'stroke="{plane_colors.get(plane, "#495057")}" stroke-width="3.0" '
                    f'stroke-linecap="round" stroke-linejoin="round" opacity="0.85"/>'
                )

        for sat, sat_rows in sorted(by_sat.items()):
            if not sat_rows:
                continue
            plane = sat_to_plane[sat]
            color = plane_colors.get(plane, "#495057")
            start = sat_rows[0]
            end = sat_rows[-1]
            start_x, start_y = map_xy(float(start["anchor_east_m"]), float(start["anchor_north_m"]))
            end_x, end_y = map_xy(float(end["anchor_east_m"]), float(end["anchor_north_m"]))

            lines.append(
                f'<circle cx="{start_x:.2f}" cy="{start_y:.2f}" r="5.8" fill="#ffffff" '
                f'stroke="{color}" stroke-width="2.0"/>'
            )
            lines.append(
                f'<rect x="{end_x - 4.8:.2f}" y="{end_y - 4.8:.2f}" width="9.6" height="9.6" '
                f'fill="{color}" stroke="#111111" stroke-width="0.8"/>'
            )
            lines.append(
                f'<text x="{start_x + 7:.2f}" y="{start_y - 7:.2f}" font-size="9" fill="{color}" '
                f'font-family="monospace">sat{sat} S</text>'
            )
            lines.append(
                f'<text x="{end_x + 7:.2f}" y="{end_y + 12:.2f}" font-size="9" fill="{color}" '
                f'font-family="monospace">sat{sat} E</text>'
            )

    if ue_points:
        role_colors = {
            "center": "#d94841",
            "ring": "#2b8a3e",
            "hotspot": "#d94841",
            "boundary": "#f59f00",
            "background": "#2b8a3e",
            "line": "#5f3dc4",
        }
        legend_x = width - margin - 154
        legend_y = margin + 22
        seen_roles: List[str] = []
        for ue in ue_points:
            role = str(ue["role"])
            fill = role_colors.get(role, "#c2255c")
            cx, cy = map_xy(float(ue["east_m"]), float(ue["north_m"]))
            lines.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="4.8" fill="{fill}" stroke="#111" stroke-width="0.7"/>')
            if ue_show_labels:
                lines.append(
                    f'<text x="{cx + 6:.2f}" y="{cy - 6:.2f}" font-size="10" fill="#111" '
                    f'font-family="monospace">{ue_label_prefix}{int(ue["ue_id"])}</text>'
                )
            if role not in seen_roles:
                seen_roles.append(role)

        if seen_roles:
            for idx, role in enumerate(seen_roles):
                fill = role_colors.get(role, "#c2255c")
                ly = legend_y + idx * 22
                lines.append(f'<circle cx="{legend_x:.2f}" cy="{ly:.2f}" r="4.8" fill="{fill}" stroke="#111" stroke-width="0.7"/>')
                lines.append(
                    f'<text x="{legend_x + 14:.2f}" y="{ly + 4:.2f}" font-size="11" fill="#111" '
                    f'font-family="monospace">{role}</text>'
                )

    if sat_anchor_points:
        plane_colors = {
            0: "#d9480f",
            1: "#1c7ed6",
        }
        legend_x = margin
        legend_y = height - margin + 10
        for idx, plane in enumerate(sorted({int(p["plane"]) for p in sat_anchor_points})):
            ly = legend_y + idx * 22
            color = plane_colors.get(plane, "#495057")
            lines.append(
                f'<line x1="{legend_x:.2f}" y1="{ly:.2f}" x2="{legend_x + 24:.2f}" y2="{ly:.2f}" '
                f'stroke="{color}" stroke-width="3.0" stroke-linecap="round"/>'
            )
            lines.append(
                f'<text x="{legend_x + 32:.2f}" y="{ly + 4:.2f}" font-size="11" fill="#111" '
                f'font-family="monospace">plane{plane} mainline</text>'
            )
        marker_y = legend_y + max(1, len({int(p["plane"]) for p in sat_anchor_points})) * 22
        lines.append(
            f'<circle cx="{legend_x + 5:.2f}" cy="{marker_y:.2f}" r="5.8" fill="#ffffff" '
            f'stroke="#111111" stroke-width="1.5"/>'
        )
        lines.append(
            f'<text x="{legend_x + 18:.2f}" y="{marker_y + 4:.2f}" font-size="11" fill="#111" '
            f'font-family="monospace">sat start</text>'
        )
        lines.append(
            f'<rect x="{legend_x + 108:.2f}" y="{marker_y - 4.8:.2f}" width="9.6" height="9.6" '
            f'fill="#111111" stroke="#111111" stroke-width="0.8"/>'
        )
        lines.append(
            f'<text x="{legend_x + 124:.2f}" y="{marker_y + 4:.2f}" font-size="11" fill="#111" '
            f'font-family="monospace">sat end</text>'
        )

    lines.append("</svg>")
    out_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    p = argparse.ArgumentParser(description="Render hex grid CSV to SVG")
    p.add_argument("--csv", required=True, type=Path, help="Input hex grid CSV path")
    p.add_argument("--out", type=Path, help="Output SVG path")
    p.add_argument("--width", type=int, default=1600)
    p.add_argument("--height", type=int, default=1200)
    p.add_argument("--margin", type=int, default=80)
    p.add_argument("--show-labels", type=_parse_bool, default=True)
    p.add_argument("--label-step", type=int, default=1, help="Show one label every N cells")
    p.add_argument("--show-centers", type=_parse_bool, default=True)
    p.add_argument("--title", default="WGS84 Hex Grid")
    p.add_argument("--subtitle", default="")
    p.add_argument("--ue-csv", type=Path, help="Optional UE layout CSV to overlay")
    p.add_argument("--sat-anchor-csv", type=Path, help="Optional satellite anchor trace CSV to overlay")
    p.add_argument("--ue-show-labels", type=_parse_bool, default=True)
    p.add_argument("--ue-label-prefix", default="UE")
    args = p.parse_args()

    out = args.out if args.out else args.csv.with_suffix(".svg")
    cells = load_cells(args.csv)
    ues = load_ues(args.ue_csv) if args.ue_csv else None
    sat_anchor_points = load_sat_anchor_trace(args.sat_anchor_csv) if args.sat_anchor_csv else None
    render_svg(
        cells=cells,
        out_path=out,
        width=args.width,
        height=args.height,
        margin=args.margin,
        show_labels=args.show_labels,
        label_step=max(1, args.label_step),
        show_centers=args.show_centers,
        title=args.title,
        ue_points=ues,
        sat_anchor_points=sat_anchor_points,
        ue_label_prefix=args.ue_label_prefix,
        ue_show_labels=args.ue_show_labels,
        subtitle=args.subtitle,
    )
    print(f"[OK] wrote: {out}")
    print(f"[INFO] cells: {len(cells)}")
    if ues is not None:
        print(f"[INFO] UEs: {len(ues)}")
    if sat_anchor_points is not None:
        print(f"[INFO] anchor rows: {len(sat_anchor_points)}")


if __name__ == "__main__":
    main()


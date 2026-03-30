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
from typing import Dict, List, Optional, Set, Tuple


def _parse_bool(value: str) -> bool:
    v = value.strip().lower()
    if v in {"1", "true", "yes", "y", "on"}:
        return True
    if v in {"0", "false", "no", "n", "off"}:
        return False
    raise argparse.ArgumentTypeError(f"invalid bool: {value}")


def _clean_csv_lines(handle):
    for line in handle:
        yield line.replace("\0", "")


def _safe_float(value: Optional[str]) -> float:
    if value is None:
        return math.nan
    value = value.strip()
    if not value:
        return math.nan
    try:
        return float(value)
    except ValueError:
        return math.nan


def load_cells(csv_path: Path) -> List[Dict[str, float]]:
    cells: List[Dict[str, float]] = []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(_clean_csv_lines(f))
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
        reader = csv.DictReader(_clean_csv_lines(f))
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
        reader = csv.DictReader(_clean_csv_lines(f))
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
            anchor_east_m = _safe_float(row.get("anchor_east_m"))
            anchor_north_m = _safe_float(row.get("anchor_north_m"))
            if not math.isfinite(anchor_east_m) or not math.isfinite(anchor_north_m):
                continue
            time_s = _safe_float(row.get("time_s"))
            sat = row.get("sat")
            plane = row.get("plane")
            slot = row.get("slot")
            cell = row.get("cell")
            anchor_grid_id = row.get("anchor_grid_id")
            if not math.isfinite(time_s) or None in {sat, plane, slot, cell, anchor_grid_id}:
                continue
            rows.append(
                {
                    "time_s": time_s,
                    "sat": int(sat),
                    "plane": int(plane),
                    "slot": int(slot),
                    "cell": int(cell),
                    "anchor_grid_id": int(anchor_grid_id),
                    "anchor_east_m": anchor_east_m,
                    "anchor_north_m": anchor_north_m,
                }
            )
    return rows


def infer_default_ue_csv(grid_csv_path: Path) -> Optional[Path]:
    candidate = grid_csv_path.with_name("ue_layout.csv")
    return candidate if candidate.exists() else None


def assign_ues_to_cells(
    cells: List[Dict[str, float]],
    ues: Optional[List[Dict[str, float | str | int]]],
) -> Dict[int, Set[str]]:
    if not ues:
        return {}

    cell_roles: Dict[int, Set[str]] = {}
    for ue in ues:
        east = float(ue["east_m"])
        north = float(ue["north_m"])
        nearest = min(
            cells,
            key=lambda cell: (cell["east_m"] - east) ** 2 + (cell["north_m"] - north) ** 2,
        )
        cell_id = int(nearest["id"])
        cell_roles.setdefault(cell_id, set()).add(str(ue["role"]))
    return cell_roles


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
    highlight_ue_cells: bool = False,
    show_only_occupied_labels: bool = False,
    focus_occupied_cells: bool = False,
    focus_padding_hex: float = 1.2,
    legend_scale: float = 1.0,
    subtitle: str = "",
) -> None:
    occupied_cell_roles = assign_ues_to_cells(cells, ue_points) if highlight_ue_cells or focus_occupied_cells else {}
    radius_m = infer_hex_radius_m(cells)

    if focus_occupied_cells and occupied_cell_roles:
        focused_cells = [c for c in cells if int(c["id"]) in occupied_cell_roles]
        east_vals = []
        north_vals = []
        for cell in focused_cells:
            for east, north in hex_vertices(cell["east_m"], cell["north_m"], radius_m):
                east_vals.append(east)
                north_vals.append(north)
        pad_m = max(0.0, focus_padding_hex) * radius_m * 2.0
        east_vals.extend([min(east_vals) - pad_m, max(east_vals) + pad_m])
        north_vals.extend([min(north_vals) - pad_m, max(north_vals) + pad_m])
    else:
        east_vals = [c["east_m"] for c in cells]
        north_vals = [c["north_m"] for c in cells]

    if sat_anchor_points:
        east_vals.extend(float(p["anchor_east_m"]) for p in sat_anchor_points)
        north_vals.extend(float(p["anchor_north_m"]) for p in sat_anchor_points)
    min_e, max_e = min(east_vals), max(east_vals)
    min_n, max_n = min(north_vals), max(north_vals)
    span_e = max(max_e - min_e, 1.0)
    span_n = max(max_n - min_n, 1.0)

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
    if title:
        lines.append(
            f'<text x="{margin}" y="{margin - 12}" font-size="16" fill="#111111" '
            f'font-family="monospace">{title}</text>'
        )
    if subtitle:
        lines.append(
            f'<text x="{margin}" y="{margin + 10}" font-size="12" fill="#444444" '
            f'font-family="monospace">{subtitle}</text>'
        )

    visible_cells = cells
    if focus_occupied_cells:
        visible_cells = [
            c
            for c in cells
            if (min_e - radius_m) <= c["east_m"] <= (max_e + radius_m)
            and (min_n - radius_m) <= c["north_m"] <= (max_n + radius_m)
        ]

    for i, c in enumerate(visible_cells):
        cell_id = int(c["id"])
        fill = "#eaf3ff"
        stroke = "#4f6d8a"
        stroke_width = "1"
        opacity = "1.0"
        if highlight_ue_cells:
            roles = occupied_cell_roles.get(cell_id, set())
            fill = "#f6f8fb"
            stroke = "#c7d0da"
            stroke_width = "0.9"
            opacity = "0.82"
            if roles == {"center"}:
                fill = "#fff0db"
                stroke = "#e67700"
                stroke_width = "1.8"
                opacity = "1.0"
            elif "ring" in roles:
                fill = "#e7f5ff"
                stroke = "#1c7ed6"
                stroke_width = "1.6"
                opacity = "1.0"
            elif roles:
                fill = "#f3d9fa"
                stroke = "#ae3ec9"
                stroke_width = "1.6"
                opacity = "1.0"

        poly = hex_vertices(c["east_m"], c["north_m"], radius_m)
        lines.append(
            f'<polygon points="{map_poly(poly)}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{stroke_width}" opacity="{opacity}"/>'
        )
        cx, cy = map_xy(c["east_m"], c["north_m"])
        if show_centers:
            lines.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="1.8" fill="#1c1c1c"/>')
        should_label = show_labels and (i % max(1, label_step) == 0)
        if show_only_occupied_labels and highlight_ue_cells:
            should_label = should_label and cell_id in occupied_cell_roles
        if should_label:
            lines.append(
                f'<text x="{cx + 3:.2f}" y="{cy - 3:.2f}" font-size="9" fill="#0a1f44" '
                f'font-family="monospace">{cell_id}</text>'
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

    if highlight_ue_cells and occupied_cell_roles:
        legend_x = width - margin - 210
        legend_y = margin + 18
        legend_rect_w = 16 * legend_scale
        legend_rect_h = 12 * legend_scale
        legend_text_size = 11 * legend_scale
        cell_legend_items = [
            ("center cell", "#fff0db", "#e67700"),
            ("ring cell", "#e7f5ff", "#1c7ed6"),
            ("other grid", "#f6f8fb", "#c7d0da"),
        ]
        for idx, (label, fill, stroke) in enumerate(cell_legend_items):
            ly = legend_y + idx * (22 * legend_scale)
            lines.append(
                f'<rect x="{legend_x:.2f}" y="{ly - (7 * legend_scale):.2f}" '
                f'width="{legend_rect_w:.2f}" height="{legend_rect_h:.2f}" '
                f'fill="{fill}" stroke="{stroke}" stroke-width="1.2"/>'
            )
            lines.append(
                f'<text x="{legend_x + (24 * legend_scale):.2f}" y="{ly + (3 * legend_scale):.2f}" '
                f'font-size="{legend_text_size:.2f}" fill="#111" '
                f'font-family="monospace">{label}</text>'
            )

    if ue_points:
        role_colors = {
            "center": "#d94841",
            "ring": "#2b8a3e",
            "line": "#5f3dc4",
        }
        legend_x = width - margin - 154
        legend_y = margin + ((92 * legend_scale) if highlight_ue_cells and occupied_cell_roles else (22 * legend_scale))
        ue_legend_radius = 4.8 * legend_scale
        ue_legend_text_size = 11 * legend_scale
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
                ly = legend_y + idx * (22 * legend_scale)
                lines.append(
                    f'<circle cx="{legend_x:.2f}" cy="{ly:.2f}" r="{ue_legend_radius:.2f}" '
                    f'fill="{fill}" stroke="#111" stroke-width="0.7"/>'
                )
                lines.append(
                    f'<text x="{legend_x + (14 * legend_scale):.2f}" y="{ly + (4 * legend_scale):.2f}" '
                    f'font-size="{ue_legend_text_size:.2f}" fill="#111" '
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
    p.add_argument("--highlight-ue-cells", type=_parse_bool, default=False)
    p.add_argument("--show-only-occupied-labels", type=_parse_bool, default=False)
    p.add_argument("--focus-occupied-cells", type=_parse_bool, default=False)
    p.add_argument("--focus-padding-hex", type=float, default=1.2)
    p.add_argument("--legend-scale", type=float, default=1.0)
    args = p.parse_args()

    out = args.out if args.out else args.csv.with_suffix(".svg")
    cells = load_cells(args.csv)
    ue_csv = args.ue_csv if args.ue_csv else infer_default_ue_csv(args.csv)
    ues = load_ues(ue_csv) if ue_csv else None
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
        highlight_ue_cells=args.highlight_ue_cells,
        show_only_occupied_labels=args.show_only_occupied_labels,
        focus_occupied_cells=args.focus_occupied_cells,
        focus_padding_hex=args.focus_padding_hex,
        legend_scale=max(0.5, args.legend_scale),
        subtitle=args.subtitle,
    )
    print(f"[OK] wrote: {out}")
    print(f"[INFO] cells: {len(cells)}")
    if ues is not None:
        print(f"[INFO] UEs: {len(ues)}")
        if ue_csv is not None:
            print(f"[INFO] UE csv: {ue_csv}")
    if sat_anchor_points is not None:
        print(f"[INFO] anchor rows: {len(sat_anchor_points)}")


if __name__ == "__main__":
    main()


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
from typing import Dict, List, Tuple


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
) -> None:
    east_vals = [c["east_m"] for c in cells]
    north_vals = [c["north_m"] for c in cells]
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
    args = p.parse_args()

    out = args.out if args.out else args.csv.with_suffix(".svg")
    cells = load_cells(args.csv)
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
    )
    print(f"[OK] wrote: {out}")
    print(f"[INFO] cells: {len(cells)}")


if __name__ == "__main__":
    main()


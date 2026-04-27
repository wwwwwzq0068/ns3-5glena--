#!/usr/bin/env python3
"""
Plot WGS84 hex grid CSV into an SVG image without third-party dependencies.

Input CSV columns expected:
id, latitude_deg, longitude_deg, altitude_m, east_m, north_m, ecef_x, ecef_y, ecef_z
"""

from __future__ import annotations

import argparse
import csv
import json
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


def _parse_plane_north_offsets(value: str) -> Dict[int, float]:
    offsets: Dict[int, float] = {}
    spec = value.strip()
    if not spec:
        return offsets
    for item in spec.split(","):
        entry = item.strip()
        if not entry:
            continue
        parts = entry.split(":", 1)
        if len(parts) != 2:
            raise argparse.ArgumentTypeError(
                f"invalid plane north offset entry {entry!r}; expected plane:offset_m"
            )
        plane_text, offset_text = parts
        try:
            plane = int(plane_text.strip())
            offset_m = float(offset_text.strip())
        except ValueError as exc:
            raise argparse.ArgumentTypeError(
                f"invalid plane north offset entry {entry!r}; expected plane:offset_m"
            ) from exc
        offsets[plane] = offset_m
    return offsets


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


def load_sat_anchor_trace(
    csv_path: Path,
    plane_north_offsets_m: Optional[Dict[int, float]] = None,
) -> List[Dict[str, float | int]]:
    rows: List[Dict[str, float | int]] = []
    offsets = plane_north_offsets_m or {}
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(_clean_csv_lines(f))
        required = {
            "time_s",
            "sat",
            "plane",
            "slot",
            "cell",
            "anchor_grid_id",
            "anchor_latitude_deg",
            "anchor_longitude_deg",
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
            plane_idx = int(plane)
            rows.append(
                {
                    "time_s": time_s,
                    "sat": int(sat),
                    "plane": plane_idx,
                    "slot": int(slot),
                    "cell": int(cell),
                    "anchor_grid_id": int(anchor_grid_id),
                    "anchor_latitude_deg": _safe_float(row.get("anchor_latitude_deg")),
                    "anchor_longitude_deg": _safe_float(row.get("anchor_longitude_deg")),
                    "anchor_east_m": anchor_east_m,
                    "anchor_north_m": anchor_north_m + offsets.get(plane_idx, 0.0),
                }
            )
    return rows


def load_sat_ground_track(
    csv_path: Path,
    plane_north_offsets_m: Optional[Dict[int, float]] = None,
) -> List[Dict[str, float | int]]:
    rows: List[Dict[str, float | int]] = []
    offsets = plane_north_offsets_m or {}
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(_clean_csv_lines(f))
        required = {
            "time_s",
            "sat",
            "plane",
            "slot",
            "cell",
            "subpoint_latitude_deg",
            "subpoint_longitude_deg",
            "subpoint_east_m",
            "subpoint_north_m",
            "sat_ecef_x",
            "sat_ecef_y",
            "sat_ecef_z",
        }
        missing = required.difference(reader.fieldnames or [])
        if missing:
            raise ValueError(f"missing required satellite ground-track columns: {sorted(missing)}")
        for row in reader:
            time_s = _safe_float(row.get("time_s"))
            sat = row.get("sat")
            plane = row.get("plane")
            slot = row.get("slot")
            cell = row.get("cell")
            east_m = _safe_float(row.get("subpoint_east_m"))
            north_m = _safe_float(row.get("subpoint_north_m"))
            lat_deg = _safe_float(row.get("subpoint_latitude_deg"))
            lon_deg = _safe_float(row.get("subpoint_longitude_deg"))
            sat_ecef_x = _safe_float(row.get("sat_ecef_x"))
            sat_ecef_y = _safe_float(row.get("sat_ecef_y"))
            sat_ecef_z = _safe_float(row.get("sat_ecef_z"))
            if (
                not math.isfinite(time_s)
                or None in {sat, plane, slot, cell}
                or not math.isfinite(east_m)
                or not math.isfinite(north_m)
            ):
                continue
            plane_idx = int(plane)
            rows.append(
                {
                    "time_s": time_s,
                    "sat": int(sat),
                    "plane": plane_idx,
                    "slot": int(slot),
                    "cell": int(cell),
                    "subpoint_latitude_deg": lat_deg,
                    "subpoint_longitude_deg": lon_deg,
                    "subpoint_east_m": east_m,
                    "subpoint_north_m": north_m + offsets.get(plane_idx, 0.0),
                    "sat_ecef_x": sat_ecef_x,
                    "sat_ecef_y": sat_ecef_y,
                    "sat_ecef_z": sat_ecef_z,
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
    sat_ground_track_points: Optional[List[Dict[str, float | int]]] = None,
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
    if sat_ground_track_points:
        east_vals.extend(float(p["subpoint_east_m"]) for p in sat_ground_track_points)
        north_vals.extend(float(p["subpoint_north_m"]) for p in sat_ground_track_points)
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


def _satellite_color(sat_id: int) -> str:
    palette = [
        "#d9480f",
        "#1c7ed6",
        "#2b8a3e",
        "#ae3ec9",
        "#f08c00",
        "#0b7285",
        "#c2255c",
        "#5c7cfa",
    ]
    return palette[sat_id % len(palette)]


def _build_track_dataset(
    rows: List[Dict[str, float | int]],
    *,
    east_key: str,
    north_key: str,
    lat_key: str,
    lon_key: str,
    map_xy,
) -> Tuple[Dict[str, List[Dict[str, float | int]]], Dict[str, Dict[str, int]]]:
    dataset: Dict[str, List[Dict[str, float | int]]] = {}
    sat_meta: Dict[str, Dict[str, int]] = {}
    sorted_rows = sorted(rows, key=lambda row: (int(row["sat"]), float(row["time_s"])))
    for row in sorted_rows:
        sat_id = int(row["sat"])
        sat_key = str(sat_id)
        east = float(row[east_key])
        north = float(row[north_key])
        x, y = map_xy(east, north)
        entry: Dict[str, float | int] = {
            "time_s": round(float(row["time_s"]), 6),
            "sat": sat_id,
            "plane": int(row["plane"]),
            "slot": int(row["slot"]),
            "cell": int(row["cell"]),
            "east_m": round(east, 3),
            "north_m": round(north, 3),
            "x": round(x, 3),
            "y": round(y, 3),
        }
        lat_deg = float(row[lat_key])
        lon_deg = float(row[lon_key])
        if math.isfinite(lat_deg):
            entry["lat_deg"] = round(lat_deg, 8)
        if math.isfinite(lon_deg):
            entry["lon_deg"] = round(lon_deg, 8)
        if "anchor_grid_id" in row:
            entry["anchor_grid_id"] = int(row["anchor_grid_id"])
        if "sat_ecef_x" in row:
            entry["sat_ecef_x"] = round(float(row["sat_ecef_x"]), 3)
            entry["sat_ecef_y"] = round(float(row["sat_ecef_y"]), 3)
            entry["sat_ecef_z"] = round(float(row["sat_ecef_z"]), 3)
        dataset.setdefault(sat_key, []).append(entry)
        sat_meta[sat_key] = {
            "sat": sat_id,
            "plane": int(row["plane"]),
            "slot": int(row["slot"]),
            "cell": int(row["cell"]),
        }
    return dataset, sat_meta


def render_html_report(
    cells: List[Dict[str, float]],
    out_path: Path,
    width: int,
    height: int,
    margin: int,
    title: str,
    subtitle: str = "",
    ue_points: Optional[List[Dict[str, float | str | int]]] = None,
    sat_anchor_points: Optional[List[Dict[str, float | int]]] = None,
    sat_ground_track_points: Optional[List[Dict[str, float | int]]] = None,
) -> None:
    if not sat_anchor_points and not sat_ground_track_points:
        raise ValueError("HTML report requires at least one of sat-anchor-csv or sat-ground-track-csv")

    east_vals = [c["east_m"] for c in cells]
    north_vals = [c["north_m"] for c in cells]
    if sat_anchor_points:
        east_vals.extend(float(p["anchor_east_m"]) for p in sat_anchor_points)
        north_vals.extend(float(p["anchor_north_m"]) for p in sat_anchor_points)
    if sat_ground_track_points:
        east_vals.extend(float(p["subpoint_east_m"]) for p in sat_ground_track_points)
        north_vals.extend(float(p["subpoint_north_m"]) for p in sat_ground_track_points)

    min_e, max_e = min(east_vals), max(east_vals)
    min_n, max_n = min(north_vals), max(north_vals)
    span_e = max(max_e - min_e, 1.0)
    span_n = max(max_n - min_n, 1.0)
    plot_w = max(1, width - 2 * margin)
    plot_h = max(1, height - 2 * margin)
    s = min(plot_w / span_e, plot_h / span_n)
    radius_m = infer_hex_radius_m(cells)

    def map_xy(east: float, north: float) -> Tuple[float, float]:
        x = margin + (east - min_e) * s
        y = height - margin - (north - min_n) * s
        return x, y

    def map_poly(poly: List[Tuple[float, float]]) -> str:
        return " ".join(
            f"{x:.2f},{y:.2f}" for x, y in (map_xy(point_east, point_north) for point_east, point_north in poly)
        )

    grid_parts: List[str] = []
    for cell in cells:
        cell_id = int(cell["id"])
        grid_parts.append(
            f'<polygon points="{map_poly(hex_vertices(cell["east_m"], cell["north_m"], radius_m))}" '
            f'fill="#edf4ff" stroke="#6d87a8" stroke-width="1.0"/>'
        )
        cx, cy = map_xy(cell["east_m"], cell["north_m"])
        grid_parts.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="1.8" fill="#1c1c1c"/>')
        grid_parts.append(
            f'<text x="{cx + 3:.2f}" y="{cy - 3:.2f}" font-size="9" fill="#0a1f44" '
            f'font-family="monospace">{cell_id}</text>'
        )
    grid_markup = "\n".join(grid_parts)

    anchor_track_data: Dict[str, List[Dict[str, float | int]]] = {}
    ground_track_data: Dict[str, List[Dict[str, float | int]]] = {}
    sat_meta: Dict[str, Dict[str, int]] = {}
    if sat_anchor_points:
        anchor_track_data, anchor_meta = _build_track_dataset(
            sat_anchor_points,
            east_key="anchor_east_m",
            north_key="anchor_north_m",
            lat_key="anchor_latitude_deg",
            lon_key="anchor_longitude_deg",
            map_xy=map_xy,
        )
        sat_meta.update(anchor_meta)
    if sat_ground_track_points:
        ground_track_data, ground_meta = _build_track_dataset(
            sat_ground_track_points,
            east_key="subpoint_east_m",
            north_key="subpoint_north_m",
            lat_key="subpoint_latitude_deg",
            lon_key="subpoint_longitude_deg",
            map_xy=map_xy,
        )
        sat_meta.update(ground_meta)

    sat_colors = {sat_key: _satellite_color(int(sat_key)) for sat_key in sat_meta}
    all_times = sorted(
        {
            round(float(row["time_s"]), 6)
            for series in list(anchor_track_data.values()) + list(ground_track_data.values())
            for row in series
        }
    )
    default_time_index = max(0, len(all_times) - 1)
    missing_messages: List[str] = []
    if not ground_track_data:
        missing_messages.append("Real track unavailable: sat_ground_track.csv missing or empty.")
    if not anchor_track_data:
        missing_messages.append("Beam landing track unavailable: sat_anchor_trace.csv missing or empty.")

    satellite_control_markup = "\n".join(
        (
            f'<label class="sat-toggle" style="--sat-color:{sat_colors[sat_key]}">'
            f'<input type="checkbox" class="sat-checkbox" data-sat="{sat_key}" checked> '
            f'<span>sat{sat_key}</span></label>'
        )
        for sat_key in sorted(sat_meta.keys(), key=lambda value: int(value))
    )

    ue_payload: List[Dict[str, float | int | str]] = []
    if ue_points:
        role_colors = {
            "center": "#d94841",
            "ring": "#2b8a3e",
            "line": "#5f3dc4",
        }
        for ue in ue_points:
            ue_label = f"UE{int(ue['ue_id'])}"
            x, y = map_xy(float(ue["east_m"]), float(ue["north_m"]))
            ue_payload.append(
                {
                    "ue_id": int(ue["ue_id"]),
                    "label": ue_label,
                    "role": str(ue["role"]),
                    "east_m": round(float(ue["east_m"]), 3),
                    "north_m": round(float(ue["north_m"]), 3),
                    "x": round(x, 3),
                    "y": round(y, 3),
                    "color": role_colors.get(str(ue["role"]), "#c2255c"),
                }
            )

    html = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Satellite Trajectory Report</title>
  <style>
    :root {
      --bg: #f6f8fb;
      --panel: #ffffff;
      --text: #111827;
      --muted: #5b6472;
      --line: #d9e1ec;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      background: linear-gradient(180deg, #f8fbff 0%%, #eef3f8 100%%);
      color: var(--text);
    }
    .page {
      display: grid;
      grid-template-columns: 320px minmax(0, 1fr);
      min-height: 100vh;
    }
    .sidebar {
      border-right: 1px solid var(--line);
      background: rgba(255, 255, 255, 0.92);
      backdrop-filter: blur(10px);
      padding: 18px 18px 24px 18px;
      overflow-y: auto;
    }
    .sidebar h1 {
      margin: 0 0 8px 0;
      font-size: 18px;
    }
    .subtitle {
      margin: 0 0 18px 0;
      color: var(--muted);
      line-height: 1.5;
      font-size: 12px;
    }
    .controls-section {
      margin-bottom: 18px;
      padding: 12px;
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 12px;
    }
    .controls-section h2 {
      margin: 0 0 10px 0;
      font-size: 13px;
    }
    .button-row {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      margin-bottom: 10px;
    }
    button {
      border: 1px solid #c9d3e1;
      background: #fff;
      color: var(--text);
      border-radius: 10px;
      padding: 7px 10px;
      cursor: pointer;
      font: inherit;
      font-size: 12px;
    }
    button:hover {
      background: #f2f6fb;
    }
    .sat-list {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 8px;
    }
    .sat-toggle {
      display: flex;
      align-items: center;
      gap: 8px;
      padding: 8px;
      border: 1px solid var(--line);
      border-left: 5px solid var(--sat-color);
      border-radius: 10px;
      background: #fbfdff;
      font-size: 12px;
    }
    .toggle-list {
      display: grid;
      gap: 8px;
      font-size: 12px;
    }
    .toggle-list label {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .time-row {
      display: grid;
      gap: 8px;
    }
    .time-label {
      color: var(--muted);
      font-size: 12px;
    }
    input[type="range"] {
      width: 100%%;
    }
    .notice {
      margin-bottom: 10px;
      padding: 9px 10px;
      border-radius: 10px;
      background: #fff4e6;
      border: 1px solid #ffd8a8;
      color: #9c5200;
      font-size: 12px;
      line-height: 1.5;
    }
    .info-panel {
      min-height: 120px;
      border: 1px solid var(--line);
      border-radius: 10px;
      background: #fbfdff;
      padding: 10px;
      font-size: 12px;
      line-height: 1.55;
      white-space: pre-line;
    }
    .canvas-wrap {
      padding: 16px;
    }
    .canvas-card {
      background: rgba(255, 255, 255, 0.9);
      border: 1px solid var(--line);
      border-radius: 18px;
      overflow: hidden;
      box-shadow: 0 18px 60px rgba(34, 66, 102, 0.08);
    }
    svg {
      display: block;
      width: 100%%;
      height: auto;
      background: #ffffff;
    }
    .legend {
      display: flex;
      gap: 16px;
      flex-wrap: wrap;
      padding: 12px 16px 16px 16px;
      border-top: 1px solid var(--line);
      color: var(--muted);
      font-size: 12px;
    }
    .legend-item {
      display: inline-flex;
      align-items: center;
      gap: 8px;
    }
    .legend-line {
      width: 26px;
      height: 0;
      border-top: 3px solid currentColor;
    }
    .legend-line.real {
      border-top-style: dashed;
    }
    .legend-dot {
      width: 10px;
      height: 10px;
      border-radius: 999px;
      border: 2px solid currentColor;
    }
    .legend-square {
      width: 10px;
      height: 10px;
      border: 2px solid currentColor;
    }
  </style>
</head>
<body>
  <div class="page">
    <aside class="sidebar">
      <h1>Satellite Trajectory Report</h1>
      <p class="subtitle">__PAGE_SUBTITLE__</p>
      __NOTICE_BLOCK__
      <section class="controls-section">
        <h2>Selection</h2>
        <div class="button-row">
          <button id="show-all" type="button">Show all</button>
          <button id="clear-all" type="button">Clear all</button>
        </div>
        <div class="sat-list">
          __SATELLITE_CONTROL_BLOCK__
        </div>
      </section>
      <section class="controls-section">
        <h2>Layers</h2>
        <div class="toggle-list">
          <label><input id="toggle-real" type="checkbox" checked> <span>Real track</span></label>
          <label><input id="toggle-anchor" type="checkbox" checked> <span>Beam landing track</span></label>
          <label><input id="toggle-ue" type="checkbox" checked> <span>UE distribution</span></label>
          <label><input id="toggle-current" type="checkbox" checked> <span>Current markers</span></label>
          <label><input id="toggle-final" type="checkbox" checked> <span>Final markers</span></label>
        </div>
      </section>
      <section class="controls-section">
        <h2>Time</h2>
        <div class="time-row">
          <div class="time-label">Current time: <strong id="time-value">0.000 s</strong></div>
          <input id="time-slider" type="range" min="0" max="0" step="1" value="0">
        </div>
      </section>
      <section class="controls-section">
        <h2>Hover Info</h2>
        <div id="info-panel" class="info-panel">Hover a line or marker to inspect a satellite.</div>
      </section>
    </aside>
    <main class="canvas-wrap">
      <div class="canvas-card">
        <svg id="plot" viewBox="0 0 __SVG_WIDTH__ __SVG_HEIGHT__" aria-label="satellite trajectory plot">
          <rect x="0" y="0" width="100%%" height="100%%" fill="#ffffff"></rect>
          <text x="__SVG_MARGIN__" y="58" font-size="20" fill="#111111" font-family="monospace">__TITLE_RAW__</text>
          <text x="__SVG_MARGIN__" y="78" font-size="12" fill="#475569" font-family="monospace">__SUBTITLE_RAW__</text>
          <g id="grid-layer">
            __GRID_MARKUP__
          </g>
          <g id="ue-layer"></g>
          <g id="track-layer"></g>
          <g id="marker-layer"></g>
        </svg>
        <div class="legend">
          <span class="legend-item"><span class="legend-line real"></span><span>real track</span></span>
          <span class="legend-item"><span class="legend-line"></span><span>beam landing track</span></span>
          <span class="legend-item"><span class="legend-dot"></span><span>current real / final real</span></span>
          <span class="legend-item"><span class="legend-square"></span><span>current beam / final beam</span></span>
        </div>
      </div>
    </main>
  </div>
  <script>
    const groundTrackData = GROUND_TRACK_DATA;
    const anchorTrackData = ANCHOR_TRACK_DATA;
    const ueDistribution = UE_DISTRIBUTION;
    const satMeta = SAT_META;
    const satColors = SAT_COLORS;
    const timeValues = TIME_VALUES;
    const defaultTimeIndex = DEFAULT_TIME_INDEX;

    const plot = document.getElementById("plot");
    const ueLayer = document.getElementById("ue-layer");
    const trackLayer = document.getElementById("track-layer");
    const markerLayer = document.getElementById("marker-layer");
    const infoPanel = document.getElementById("info-panel");
    const timeSlider = document.getElementById("time-slider");
    const timeValue = document.getElementById("time-value");
    const showAllBtn = document.getElementById("show-all");
    const clearAllBtn = document.getElementById("clear-all");
    const toggleReal = document.getElementById("toggle-real");
    const toggleAnchor = document.getElementById("toggle-anchor");
    const toggleUe = document.getElementById("toggle-ue");
    const toggleCurrent = document.getElementById("toggle-current");
    const toggleFinal = document.getElementById("toggle-final");
    const satCheckboxes = Array.from(document.querySelectorAll(".sat-checkbox"));
    const svgNs = "http://www.w3.org/2000/svg";
    const allSatIds = Object.keys(satMeta).sort((a, b) => Number(a) - Number(b));

    function selectedSatIds() {
      return satCheckboxes.filter((item) => item.checked).map((item) => item.dataset.sat);
    }

    function formatPoint(prefix, point, includeGrid) {
      const lines = [];
      lines.push(`${prefix} time: ${Number(point.time_s).toFixed(3)} s`);
      lines.push(`${prefix} east/north: ${Number(point.east_m).toFixed(1)} m, ${Number(point.north_m).toFixed(1)} m`);
      if (point.lat_deg !== undefined && point.lon_deg !== undefined) {
        lines.push(`${prefix} lat/lon: ${Number(point.lat_deg).toFixed(6)}°, ${Number(point.lon_deg).toFixed(6)}°`);
      }
      if (includeGrid && point.anchor_grid_id !== undefined) {
        lines.push(`${prefix} grid: ${point.anchor_grid_id}`);
      }
      return lines;
    }

    function setInfo(title, lines) {
      infoPanel.textContent = [title].concat(lines).join("\\n");
    }

    function nearestPointAtOrBefore(series, currentTime) {
      if (!series || series.length === 0) {
        return null;
      }
      let candidate = series[0];
      for (const point of series) {
        if (Number(point.time_s) <= currentTime + 1e-9) {
          candidate = point;
        } else {
          break;
        }
      }
      return candidate;
    }

    function seriesToPoints(series) {
      return series.map((point) => `${point.x},${point.y}`).join(" ");
    }

    function addHover(target, satId, layerName, point, includeGrid) {
      target.addEventListener("mouseenter", () => {
        const meta = satMeta[satId];
        const lines = [
          `plane=${meta.plane} slot=${meta.slot} cell=${meta.cell}`,
          `layer=${layerName}`,
        ].concat(formatPoint(layerName, point, includeGrid));
        setInfo(`sat${satId}`, lines);
      });
    }

    function appendPolyline(parent, satId, color, series, dashed, layerName, includeGrid) {
      if (!series || series.length < 2) {
        return;
      }
      const polyline = document.createElementNS(svgNs, "polyline");
      polyline.setAttribute("points", seriesToPoints(series));
      polyline.setAttribute("fill", "none");
      polyline.setAttribute("stroke", color);
      polyline.setAttribute("stroke-width", dashed ? "2.4" : "3.2");
      polyline.setAttribute("stroke-linecap", "round");
      polyline.setAttribute("stroke-linejoin", "round");
      polyline.setAttribute("opacity", dashed ? "0.72" : "0.92");
      if (dashed) {
        polyline.setAttribute("stroke-dasharray", "8 6");
      }
      addHover(polyline, satId, layerName, series[series.length - 1], includeGrid);
      parent.appendChild(polyline);
    }

    function appendCircle(parent, satId, color, point, filled, layerName) {
      const circle = document.createElementNS(svgNs, "circle");
      circle.setAttribute("cx", point.x);
      circle.setAttribute("cy", point.y);
      circle.setAttribute("r", filled ? "6.5" : "8");
      circle.setAttribute("fill", filled ? color : "#ffffff");
      circle.setAttribute("stroke", color);
      circle.setAttribute("stroke-width", "2.2");
      addHover(circle, satId, layerName, point, false);
      parent.appendChild(circle);
    }

    function appendSquare(parent, satId, color, point, filled, layerName) {
      const rect = document.createElementNS(svgNs, "rect");
      const size = filled ? 10 : 12;
      rect.setAttribute("x", Number(point.x) - size / 2);
      rect.setAttribute("y", Number(point.y) - size / 2);
      rect.setAttribute("width", size);
      rect.setAttribute("height", size);
      rect.setAttribute("fill", filled ? color : "#ffffff");
      rect.setAttribute("stroke", color);
      rect.setAttribute("stroke-width", "2.2");
      addHover(rect, satId, layerName, point, true);
      parent.appendChild(rect);
    }

    function renderUes() {
      ueLayer.innerHTML = "";
      if (!toggleUe.checked) {
        return;
      }
      for (const ue of ueDistribution) {
        const circle = document.createElementNS(svgNs, "circle");
        circle.setAttribute("cx", ue.x);
        circle.setAttribute("cy", ue.y);
        circle.setAttribute("r", "4.2");
        circle.setAttribute("fill", ue.color);
        circle.setAttribute("stroke", "#102030");
        circle.setAttribute("stroke-width", "0.8");
        circle.addEventListener("mouseenter", () => {
          setInfo(ue.label, [
            `role=${ue.role}`,
            `east/north=${Number(ue.east_m).toFixed(1)} m, ${Number(ue.north_m).toFixed(1)} m`,
          ]);
        });
        ueLayer.appendChild(circle);

        const label = document.createElementNS(svgNs, "text");
        label.setAttribute("x", Number(ue.x) + 6);
        label.setAttribute("y", Number(ue.y) - 6);
        label.setAttribute("font-size", "10");
        label.setAttribute("fill", "#111111");
        label.setAttribute("font-family", "monospace");
        label.textContent = ue.label;
        ueLayer.appendChild(label);
      }
    }

    function render() {
      renderUes();
      trackLayer.innerHTML = "";
      markerLayer.innerHTML = "";

      const selected = new Set(selectedSatIds());
      const currentIndex = Math.min(Number(timeSlider.value || 0), Math.max(timeValues.length - 1, 0));
      const currentTime = timeValues.length > 0 ? Number(timeValues[currentIndex]) : 0;
      timeValue.textContent = `${currentTime.toFixed(3)} s`;

      for (const satId of allSatIds) {
        if (!selected.has(satId)) {
          continue;
        }
        const color = satColors[satId] || "#495057";
        const realSeries = groundTrackData[satId] || [];
        const anchorSeries = anchorTrackData[satId] || [];

        if (toggleReal.checked) {
          appendPolyline(trackLayer, satId, color, realSeries, true, "real track", false);
          if (toggleCurrent.checked) {
            const currentReal = nearestPointAtOrBefore(realSeries, currentTime);
            if (currentReal) {
              appendCircle(markerLayer, satId, color, currentReal, true, "current real");
            }
          }
          if (toggleFinal.checked && realSeries.length > 0) {
            appendCircle(markerLayer, satId, color, realSeries[realSeries.length - 1], false, "final real");
          }
        }

        if (toggleAnchor.checked) {
          appendPolyline(trackLayer, satId, color, anchorSeries, false, "beam landing track", true);
          if (toggleCurrent.checked) {
            const currentAnchor = nearestPointAtOrBefore(anchorSeries, currentTime);
            if (currentAnchor) {
              appendSquare(markerLayer, satId, color, currentAnchor, true, "current beam");
            }
          }
          if (toggleFinal.checked && anchorSeries.length > 0) {
            appendSquare(markerLayer, satId, color, anchorSeries[anchorSeries.length - 1], false, "final beam");
          }
        }
      }
    }

    timeSlider.min = "0";
    timeSlider.max = String(Math.max(timeValues.length - 1, 0));
    timeSlider.value = String(defaultTimeIndex);

    showAllBtn.addEventListener("click", () => {
      satCheckboxes.forEach((checkbox) => { checkbox.checked = true; });
      render();
    });
    clearAllBtn.addEventListener("click", () => {
      satCheckboxes.forEach((checkbox) => { checkbox.checked = false; });
      render();
    });
    satCheckboxes.forEach((checkbox) => checkbox.addEventListener("change", render));
    timeSlider.addEventListener("input", render);
    [toggleReal, toggleAnchor, toggleUe, toggleCurrent, toggleFinal].forEach((checkbox) => {
      checkbox.addEventListener("change", render);
    });

    render();
  </script>
</body>
</html>
"""
    notice_block = ""
    if missing_messages:
        notice_block = "\n".join(
            f'<div class="notice">{message}</div>' for message in missing_messages
        )
    html = html.replace("const anchorTrackData = ANCHOR_TRACK_DATA;", f"const anchorTrackData = {json.dumps(anchor_track_data, separators=(',', ':'))};")
    html = html.replace("const satMeta = SAT_META;", f"const satMeta = {json.dumps(sat_meta, separators=(',', ':'))};")
    html = html.replace("const satColors = SAT_COLORS;", f"const satColors = {json.dumps(sat_colors, separators=(',', ':'))};")
    html = html.replace("const timeValues = TIME_VALUES;", f"const timeValues = {json.dumps(all_times, separators=(',', ':'))};")
    html = html.replace("const defaultTimeIndex = DEFAULT_TIME_INDEX;", f"const defaultTimeIndex = {default_time_index};")
    html = html.replace("const groundTrackData = GROUND_TRACK_DATA;", f"const groundTrackData = {json.dumps(ground_track_data, separators=(',', ':'))};")
    html = html.replace("const ueDistribution = UE_DISTRIBUTION;", f"const ueDistribution = {json.dumps(ue_payload, separators=(',', ':'))};")
    html = (
        html.replace("__PAGE_SUBTITLE__", subtitle or title)
        .replace("__NOTICE_BLOCK__", notice_block)
        .replace("__SATELLITE_CONTROL_BLOCK__", satellite_control_markup)
        .replace("__SVG_WIDTH__", str(width))
        .replace("__SVG_HEIGHT__", str(height))
        .replace("__SVG_MARGIN__", str(margin))
        .replace("__TITLE_RAW__", title)
        .replace("__SUBTITLE_RAW__", subtitle)
        .replace("__GRID_MARKUP__", grid_markup)
    )
    out_path.write_text(html, encoding="utf-8")


def main() -> None:
    p = argparse.ArgumentParser(description="Render hex grid CSV to SVG")
    p.add_argument("--csv", required=True, type=Path, help="Input hex grid CSV path")
    p.add_argument("--out", type=Path, help="Output SVG path")
    p.add_argument("--html-out", type=Path, help="Output interactive HTML path")
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
    p.add_argument("--sat-ground-track-csv", type=Path, help="Optional continuous satellite ground-track CSV to overlay")
    p.add_argument("--ue-show-labels", type=_parse_bool, default=True)
    p.add_argument("--ue-label-prefix", default="UE")
    p.add_argument("--highlight-ue-cells", type=_parse_bool, default=False)
    p.add_argument("--show-only-occupied-labels", type=_parse_bool, default=False)
    p.add_argument("--focus-occupied-cells", type=_parse_bool, default=False)
    p.add_argument("--focus-padding-hex", type=float, default=1.2)
    p.add_argument("--legend-scale", type=float, default=1.0)
    p.add_argument(
        "--plane-north-offsets-m",
        type=_parse_plane_north_offsets,
        default=None,
        help="Optional display-only track north offsets like '0:-12000,1:5000'",
    )
    args = p.parse_args()

    out = args.out if args.out else args.csv.with_suffix(".svg")
    html_out = args.html_out
    cells = load_cells(args.csv)
    ue_csv = args.ue_csv if args.ue_csv else infer_default_ue_csv(args.csv)
    ues = load_ues(ue_csv) if ue_csv else None
    plane_north_offsets_m = args.plane_north_offsets_m or {}
    sat_anchor_points = (
        load_sat_anchor_trace(args.sat_anchor_csv, plane_north_offsets_m)
        if args.sat_anchor_csv
        else None
    )
    sat_ground_track_points = (
        load_sat_ground_track(args.sat_ground_track_csv, plane_north_offsets_m)
        if args.sat_ground_track_csv
        else None
    )
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
        sat_ground_track_points=sat_ground_track_points,
        ue_label_prefix=args.ue_label_prefix,
        ue_show_labels=args.ue_show_labels,
        highlight_ue_cells=args.highlight_ue_cells,
        show_only_occupied_labels=args.show_only_occupied_labels,
        focus_occupied_cells=args.focus_occupied_cells,
        focus_padding_hex=args.focus_padding_hex,
        legend_scale=max(0.5, args.legend_scale),
        subtitle=args.subtitle,
    )
    if html_out is not None:
        render_html_report(
            cells=cells,
            out_path=html_out,
            width=args.width,
            height=args.height,
            margin=args.margin,
            title=args.title,
            subtitle=args.subtitle,
            ue_points=ues,
            sat_anchor_points=sat_anchor_points,
            sat_ground_track_points=sat_ground_track_points,
        )
    print(f"[OK] wrote: {out}")
    if html_out is not None:
        print(f"[OK] wrote: {html_out}")
    print(f"[INFO] cells: {len(cells)}")
    if ues is not None:
        print(f"[INFO] UEs: {len(ues)}")
        if ue_csv is not None:
            print(f"[INFO] UE csv: {ue_csv}")
    if sat_anchor_points is not None:
        print(f"[INFO] anchor rows: {len(sat_anchor_points)}")
    if sat_ground_track_points is not None:
        print(f"[INFO] ground-track rows: {len(sat_ground_track_points)}")
    if plane_north_offsets_m:
        print(f"[INFO] plane north offsets (m): {plane_north_offsets_m}")


if __name__ == "__main__":
    main()


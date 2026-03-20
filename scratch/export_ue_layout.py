#!/usr/bin/env python3
"""
Export the current UE layout into a CSV using the same placement rules as
`scratch/leo-ntn-handover-runtime.h`.
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from typing import Iterable, List, Tuple


EARTH_RADIUS_METERS = 6378137.0


def build_line_offsets(ue_num: int, spacing_m: float) -> List[Tuple[str, float, float]]:
    offsets: List[Tuple[str, float, float]] = []
    center = (ue_num - 1.0) / 2.0
    for i in range(ue_num):
        offsets.append(("line", (i - center) * spacing_m, 0.0))
    return offsets


def build_seven_cell_offsets(
    ue_num: int,
    hex_cell_radius_m: float,
    center_spacing_m: float,
    ring_point_offset_m: float,
) -> List[Tuple[str, float, float]]:
    if ue_num != 25:
        raise ValueError("seven-cell layout currently requires ue-num == 25")

    offsets: List[Tuple[str, float, float]] = []

    for row in range(-1, 2):
        for col in range(-1, 2):
            offsets.append(("center", col * center_spacing_m, row * center_spacing_m))

    dx = math.sqrt(3.0) * hex_cell_radius_m
    dy = 1.5 * hex_cell_radius_m
    ring_centers = [
        (-dx, 0.0),
        (-0.5 * dx, dy),
        (0.5 * dx, dy),
        (dx, 0.0),
        (0.5 * dx, -dy),
        (-0.5 * dx, -dy),
    ]
    ring_counts = [3, 3, 3, 3, 2, 2]

    for (center_east_m, center_north_m), count in zip(ring_centers, ring_counts):
        norm = math.hypot(center_east_m, center_north_m)
        dir_east = center_east_m / norm
        dir_north = center_north_m / norm
        tangent_east = -dir_north
        tangent_north = dir_east

        if count == 3:
            offsets.append(
                (
                    "ring",
                    center_east_m - ring_point_offset_m * dir_east,
                    center_north_m - ring_point_offset_m * dir_north,
                )
            )
            offsets.append(
                (
                    "ring",
                    center_east_m + ring_point_offset_m * tangent_east,
                    center_north_m + ring_point_offset_m * tangent_north,
                )
            )
            offsets.append(
                (
                    "ring",
                    center_east_m - ring_point_offset_m * tangent_east,
                    center_north_m - ring_point_offset_m * tangent_north,
                )
            )
        else:
            tangential_offset_m = 0.75 * ring_point_offset_m
            offsets.append(
                (
                    "ring",
                    center_east_m + tangential_offset_m * tangent_east,
                    center_north_m + tangential_offset_m * tangent_north,
                )
            )
            offsets.append(
                (
                    "ring",
                    center_east_m - tangential_offset_m * tangent_east,
                    center_north_m - tangential_offset_m * tangent_north,
                )
            )

    return offsets


def offsets_to_rows(
    base_lat_deg: float,
    base_lon_deg: float,
    offsets: Iterable[Tuple[str, float, float]],
) -> List[dict]:
    rows: List[dict] = []
    base_lat_rad = math.radians(base_lat_deg)
    lon_scale = max(0.1, math.cos(base_lat_rad))

    for ue_id, (role, east_m, north_m) in enumerate(offsets):
        delta_lat_deg = math.degrees(north_m / EARTH_RADIUS_METERS)
        delta_lon_deg = math.degrees(east_m / (EARTH_RADIUS_METERS * lon_scale))
        rows.append(
            {
                "ue_id": ue_id,
                "role": role,
                "east_m": east_m,
                "north_m": north_m,
                "latitude_deg": base_lat_deg + delta_lat_deg,
                "longitude_deg": base_lon_deg + delta_lon_deg,
            }
        )
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description="Export UE layout CSV")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--layout-type", default="seven-cell", choices=["line", "seven-cell"])
    parser.add_argument("--ue-num", type=int, default=25)
    parser.add_argument("--ue-latitude-deg", type=float, default=45.6)
    parser.add_argument("--ue-longitude-deg", type=float, default=84.9)
    parser.add_argument("--ue-spacing-meters", type=float, default=40000.0)
    parser.add_argument("--hex-cell-radius-meters", type=float, default=20000.0)
    parser.add_argument("--ue-center-spacing-meters", type=float, default=6000.0)
    parser.add_argument("--ue-ring-point-offset-meters", type=float, default=5000.0)
    args = parser.parse_args()

    if args.layout_type == "seven-cell":
        offsets = build_seven_cell_offsets(
            ue_num=args.ue_num,
            hex_cell_radius_m=args.hex_cell_radius_meters,
            center_spacing_m=args.ue_center_spacing_meters,
            ring_point_offset_m=args.ue_ring_point_offset_meters,
        )
    else:
        offsets = build_line_offsets(args.ue_num, args.ue_spacing_meters)

    rows = offsets_to_rows(args.ue_latitude_deg, args.ue_longitude_deg, offsets)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=["ue_id", "role", "east_m", "north_m", "latitude_deg", "longitude_deg"],
        )
        writer.writeheader()
        writer.writerows(rows)

    print(f"[OK] wrote: {args.out}")
    print(f"[INFO] layout={args.layout_type} ues={len(rows)}")


if __name__ == "__main__":
    main()

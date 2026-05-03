#!/usr/bin/env python3
"""Generate the thesis Figure 4-2 LEO-NTN scenario diagram."""

from __future__ import annotations

import argparse
import csv
import math
import os
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path("scratch/results/.matplotlib-cache")))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.patches import Circle, Ellipse, FancyArrowPatch, Polygon


DEFAULT_INPUT_DIR = Path("scratch/results/formal/v6.1-poisson3ring-overlap-beamonly-40s/baseline/seed-01")
DEFAULT_OUTPUT_DIR = Path("scratch/results/formal/v6.1-poisson3ring-overlap-beamonly-40s/figures")
HEX_RADIUS_M = 20_000.0
SERVICE_AREA_RADIUS_M = 85_000.0
BLUE = "#2563eb"
LIGHT_BLUE = "#dbeafe"
GRID_BLUE = "#9fb6ce"
GRAY = "#475569"
LIGHT_GRAY = "#e2e8f0"
ORANGE = "#f59e0b"
BLACK = "#111827"
MUTED = "#64748b"
REFERENCE_UE = (-6_500.0, 10_000.0)
SATELLITES = [
    {"label": "S0", "center": (-44_000.0, 23_000.0), "beam": (58_000.0, 42_000.0, -15.0), "track_y": 68_000.0},
    {"label": "S1", "center": (-8_000.0, 35_000.0), "beam": (60_000.0, 44_000.0, -12.0), "track_y": 50_000.0},
    {"label": "S2", "center": (31_000.0, 18_000.0), "beam": (62_000.0, 42_000.0, -10.0), "track_y": 32_000.0},
    {"label": "S3", "center": (-24_000.0, -28_000.0), "beam": (56_000.0, 40_000.0, -18.0), "track_y": -10_000.0},
]


def configure_fonts() -> None:
    font_candidates = [
        "/System/Library/Fonts/STHeiti Medium.ttc",
        "/System/Library/Fonts/Supplemental/Songti.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/System/Library/Fonts/PingFang.ttc",
    ]
    for font_path in font_candidates:
        if Path(font_path).exists():
            font_manager.fontManager.addfont(font_path)
            font_name = font_manager.FontProperties(fname=font_path).get_name()
            plt.rcParams["font.family"] = font_name
            break
    plt.rcParams["axes.unicode_minus"] = False


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def as_float(row: dict[str, str], key: str) -> float:
    return float(row[key])


def hex_vertices(east_m: float, north_m: float, radius_m: float = HEX_RADIUS_M) -> list[tuple[float, float]]:
    return [
        (
            east_m + radius_m * math.cos(math.radians(30.0 + 60.0 * i)),
            north_m + radius_m * math.sin(math.radians(30.0 + 60.0 * i)),
        )
        for i in range(6)
    ]


def draw_hex_grid(ax: plt.Axes, grid_rows: list[dict[str, str]]) -> None:
    for row in grid_rows:
        east = as_float(row, "east_m")
        north = as_float(row, "north_m")
        dist = math.hypot(east, north)
        if dist > 125_000.0:
            continue
        in_service_area = dist <= SERVICE_AREA_RADIUS_M
        face = LIGHT_BLUE if in_service_area else "#f8fbff"
        edge = GRID_BLUE if in_service_area else LIGHT_GRAY
        lw = 0.95 if in_service_area else 0.45
        alpha = 0.74 if in_service_area else 0.38
        ax.add_patch(
            Polygon(
                hex_vertices(east, north),
                closed=True,
                facecolor=face,
                edgecolor=edge,
                linewidth=lw,
                alpha=alpha,
                zorder=1,
            )
        )


def draw_ues(ax: plt.Axes, ue_rows: list[dict[str, str]]) -> tuple[float, float]:
    spread = sorted(
        ue_rows,
        key=lambda row: (round(as_float(row, "north_m") / 12_000.0), round(as_float(row, "east_m") / 12_000.0)),
    )
    shown = spread[::2][:16]
    for row in shown:
        ax.scatter(
            as_float(row, "east_m"),
            as_float(row, "north_m"),
            s=28,
            c=BLACK,
            marker="o",
            edgecolors="#ffffff",
            linewidths=0.55,
            alpha=0.92,
            zorder=8,
        )
    focus = REFERENCE_UE
    ax.scatter([focus[0]], [focus[1]], s=115, facecolors=ORANGE, edgecolors=BLACK, linewidths=1.1, zorder=11)
    ax.annotate(
        "参考 UE",
        xy=focus,
        xytext=(focus[0] + 8_000, focus[1] + 12_000),
        fontsize=10.0,
        color=BLACK,
        arrowprops={"arrowstyle": "->", "lw": 0.9, "color": GRAY},
        zorder=12,
    )
    return focus


def draw_schematic_satellites(ax: plt.Axes, focus_ue: tuple[float, float]) -> None:
    for sat in SATELLITES:
        x, y = sat["center"]
        width, height, angle = sat["beam"]
        ax.add_patch(
            Ellipse(
                (x, y),
                width=width,
                height=height,
                angle=angle,
                facecolor=LIGHT_BLUE,
                edgecolor=BLUE,
                linewidth=1.15,
                alpha=0.54,
                zorder=3,
            )
        )
        ax.scatter([x], [y], marker="s", s=78, facecolors="#ffffff", edgecolors=BLUE, linewidths=1.35, zorder=9)
        ax.text(x + 5_000, y + 6_500, sat["label"], color=BLUE, fontsize=10.5, weight="bold", zorder=10)
        ax.plot([focus_ue[0], x], [focus_ue[1], y], color=ORANGE, lw=0.85, ls=(0, (3, 3)), alpha=0.58, zorder=4)

    for y_track in (69_000.0, 31_000.0):
        arrow = FancyArrowPatch(
            (-92_000.0, y_track + 14_000.0),
            (78_000.0, y_track - 42_000.0),
            arrowstyle="-|>",
            mutation_scale=12,
            linewidth=1.05,
            linestyle=(0, (5, 4)),
            color=GRAY,
            alpha=0.72,
            zorder=2,
        )
        ax.add_patch(arrow)

    ax.text(
        -66_000,
        62_000,
        "卫星地面轨迹方向",
        fontsize=9.8,
        color=MUTED,
        bbox={"boxstyle": "round,pad=0.25", "facecolor": "#ffffff", "edgecolor": LIGHT_GRAY, "alpha": 0.88},
        zorder=12,
    )


def draw_context_labels(ax: plt.Axes) -> None:
    ax.text(
        -96_000,
        -78_000,
        "边缘 UE 位于多波束重叠区，\n更容易触发星间或波束间切换",
        fontsize=10.0,
        color=GRAY,
        linespacing=1.28,
        zorder=12,
    )


def draw_inset(fig: plt.Figure, ax: plt.Axes) -> None:
    inset = ax.inset_axes([0.735, 0.79, 0.205, 0.135])
    inset.set_facecolor("#ffffff")
    for spine in inset.spines.values():
        spine.set_edgecolor("#cbd5e1")
        spine.set_linewidth(0.85)
    inset.set_xticks([])
    inset.set_yticks([])
    inset.set_title("服务星变化示意", fontsize=8.6, pad=3)
    inset.plot([0.18, 0.82], [0.54, 0.54], color=GRAY, lw=1.0, ls=(0, (4, 3)))
    inset.annotate("", xy=(0.82, 0.54), xytext=(0.18, 0.54), arrowprops={"arrowstyle": "-|>", "lw": 1.0, "color": GRAY})
    inset.scatter([0.22], [0.54], marker="s", s=52, facecolors="#ffffff", edgecolors=BLUE, linewidths=1.2, zorder=3)
    inset.scatter([0.78], [0.54], marker="s", s=52, facecolors="#ffffff", edgecolors=BLUE, linewidths=1.2, zorder=3)
    inset.text(0.22, 0.68, "t=0 s", ha="center", fontsize=7.7, color=GRAY)
    inset.text(0.78, 0.68, "t=20 s", ha="center", fontsize=7.7, color=GRAY)
    inset.text(0.22, 0.30, "S0", ha="center", fontsize=8.8, color=BLUE, weight="bold")
    inset.text(0.78, 0.30, "S2", ha="center", fontsize=8.8, color=BLUE, weight="bold")
    inset.set_xlim(0, 1)
    inset.set_ylim(0, 1)


def draw_business_flow(fig: plt.Figure) -> None:
    fig.text(
        0.5,
        0.082,
        "持续下行业务流：远端主机  →  核心网  →  低轨卫星  →  用户终端",
        ha="center",
        va="center",
        fontsize=9.4,
        color=MUTED,
    )


def make_figure(input_dir: Path, output_dir: Path) -> list[Path]:
    configure_fonts()
    grid_rows = read_rows(input_dir / "hex_grid_cells.csv")
    ue_rows = read_rows(input_dir / "ue_layout.csv")

    plt.rcParams.update(
        {
            "font.size": 10.0,
            "axes.titlesize": 14.5,
            "axes.labelsize": 10.5,
            "legend.fontsize": 9.5,
        }
    )
    fig, ax = plt.subplots(figsize=(10.4, 7.4), dpi=200)
    ax.set_facecolor("#ffffff")

    draw_hex_grid(ax, grid_rows)
    focus_ue = draw_ues(ax, ue_rows)
    draw_schematic_satellites(ax, focus_ue)
    draw_context_labels(ax)
    draw_inset(fig, ax)
    draw_business_flow(fig)

    legend_handles = [
        plt.Line2D([0], [0], color=GRAY, lw=1.2, ls=(0, (5, 4)), label="卫星地面轨迹"),
        plt.Line2D([0], [0], marker="s", color=BLUE, markerfacecolor="#ffffff", lw=0, label="波束中心"),
        plt.Line2D([0], [0], color=BLUE, lw=5.5, alpha=0.25, label="波束覆盖范围"),
        plt.Line2D([0], [0], color=ORANGE, lw=1.0, ls=(0, (3, 3)), label="候选服务链路"),
        plt.Line2D([0], [0], marker="o", color=BLACK, markerfacecolor=BLACK, lw=0, label="固定 UE"),
        plt.Line2D([0], [0], marker="o", color=BLACK, markerfacecolor=ORANGE, lw=0, label="参考 UE"),
    ]
    ax.legend(handles=legend_handles, loc="lower right", frameon=True, facecolor="#ffffff", edgecolor="#cbd5e1")

    ax.set_title("低轨卫星网络仿真场景示意图")
    ax.set_xlabel("局部东向距离 / km")
    ax.set_ylabel("局部北向距离 / km")
    ax.set_aspect("equal", adjustable="box")
    ax.grid(True, color=LIGHT_GRAY, linewidth=0.45, alpha=0.55, zorder=0)
    ax.set_xlim(-115_000, 105_000)
    ax.set_ylim(-92_000, 90_000)
    ticks = [-100_000, -50_000, 0, 50_000, 100_000]
    ax.set_xticks(ticks)
    ax.set_yticks([-80_000, -40_000, 0, 40_000, 80_000])
    ax.set_xticklabels([f"{value / 1000:.0f}" for value in ticks])
    ax.set_yticklabels([f"{value / 1000:.0f}" for value in [-80_000, -40_000, 0, 40_000, 80_000]])

    fig.text(
        0.5,
        0.025,
        "图 4-2 低轨卫星网络仿真场景示意图",
        ha="center",
        va="bottom",
        fontsize=12.0,
        color=BLACK,
    )
    fig.tight_layout(rect=[0.035, 0.105, 0.995, 0.982])

    output_dir.mkdir(parents=True, exist_ok=True)
    png = output_dir / "figure_4_2_leo_ntn_scenario.png"
    pdf = output_dir / "figure_4_2_leo_ntn_scenario.pdf"
    fig.savefig(png)
    fig.savefig(pdf)
    plt.close(fig)
    return [png, pdf]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=DEFAULT_INPUT_DIR, help="Directory containing scenario CSV outputs")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR, help="Directory for generated figure files")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    paths = make_figure(args.input_dir, args.output_dir)
    for path in paths:
        print(f"[OK] wrote: {path}")


if __name__ == "__main__":
    main()

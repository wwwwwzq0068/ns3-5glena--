#!/usr/bin/env python3
"""Generate the thesis satellite antenna model validation figure."""

from __future__ import annotations

import csv
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager
import numpy as np


OUT_DIR = Path("scratch/results/antenna-pattern")
OUT_DIR.mkdir(parents=True, exist_ok=True)

NH = 12
NV = 12
ELEMENT_MAX_GAIN_DBI = 20.0
ELEMENT_WIDTH_DEG = 4.0
MAX_ATTENUATION_DB = 30.0
DH_OVER_LAMBDA = 0.5
DV_OVER_LAMBDA = 0.5
THETA0_DEG = 90.0
PHI0_DEG = 0.0
DISPLAY_FLOOR_DBI = -20.0
COMPOSITE_PEAK_DBI = ELEMENT_MAX_GAIN_DBI + 10.0 * math.log10(NH * NV)


def configure_chinese_fonts() -> None:
    font_candidates = [
        "/System/Library/Fonts/STHeiti Medium.ttc",
        "/System/Library/Fonts/Supplemental/Songti.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
    ]
    for font_path in font_candidates:
        if Path(font_path).exists():
            font_manager.fontManager.addfont(font_path)
            font_name = font_manager.FontProperties(fname=font_path).get_name()
            plt.rcParams["font.family"] = font_name
            break
    plt.rcParams["axes.unicode_minus"] = False


def element_gain_db(theta_deg: np.ndarray, phi_deg: np.ndarray) -> np.ndarray:
    psi_deg = np.sqrt((theta_deg - THETA0_DEG) ** 2 + (phi_deg - PHI0_DEG) ** 2)
    attenuation = np.minimum(12.0 * (psi_deg / ELEMENT_WIDTH_DEG) ** 2, MAX_ATTENUATION_DB)
    return ELEMENT_MAX_GAIN_DBI - attenuation


def array_response(theta_deg: np.ndarray, phi_deg: np.ndarray) -> np.ndarray:
    theta = np.deg2rad(theta_deg)
    phi = np.deg2rad(phi_deg)
    response = np.zeros(np.shape(theta), dtype=np.complex128)
    for m in range(NH):
        for n in range(NV):
            phase = 2.0 * math.pi * (
                n * DV_OVER_LAMBDA * np.cos(theta)
                + m * DH_OVER_LAMBDA * np.sin(theta) * np.sin(phi)
            )
            response += np.exp(1j * phase)
    return response


def composite_gain_db(theta_deg: np.ndarray, phi_deg: np.ndarray) -> np.ndarray:
    # Normalized beamforming weights give a boresight array gain of 10log10(NH*NV).
    array_power_gain = np.abs(array_response(theta_deg, phi_deg)) ** 2 / (NH * NV)
    array_power_gain = np.maximum(array_power_gain, 1e-12)
    return element_gain_db(theta_deg, phi_deg) + 10.0 * np.log10(array_power_gain)


def write_cut_csv(phi_cut_deg: float = 0.0) -> Path:
    theta = np.linspace(THETA0_DEG - 30.0, THETA0_DEG + 30.0, 2401)
    phi = np.full_like(theta, phi_cut_deg)
    elem = element_gain_db(theta, phi)
    comp = composite_gain_db(theta, phi)
    path = OUT_DIR / "satellite_composite_antenna_gain_cut.csv"
    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["off_axis_theta_deg", "element_gain_dbi", "composite_gain_dbi"])
        for t, ge, gt in zip(theta - THETA0_DEG, elem, comp):
            writer.writerow([f"{t:.6f}", f"{ge:.6f}", f"{gt:.6f}"])
    return path


def plot() -> None:
    configure_chinese_fonts()

    theta_cut = np.linspace(THETA0_DEG - 30.0, THETA0_DEG + 30.0, 2401)
    phi_cut = np.zeros_like(theta_cut)
    x_cut = theta_cut - THETA0_DEG
    elem_cut = element_gain_db(theta_cut, phi_cut)
    comp_cut = composite_gain_db(theta_cut, phi_cut)

    az = np.linspace(-30.0, 30.0, 501)
    el = np.linspace(-30.0, 30.0, 501)
    az_grid, el_grid = np.meshgrid(az, el)
    heat = composite_gain_db(THETA0_DEG + el_grid, PHI0_DEG + az_grid)
    heat_display = np.maximum(heat, DISPLAY_FLOOR_DBI)

    plt.rcParams.update(
        {
            "font.size": 11.0,
            "axes.titlesize": 13.0,
            "axes.labelsize": 12.0,
            "legend.fontsize": 10.0,
        }
    )

    fig, axes = plt.subplots(1, 2, figsize=(13.0, 5.45), dpi=180)

    ax = axes[0]
    ax.plot(x_cut, comp_cut, color="#1f5fbf", lw=1.9, label=r"复合增益 $G_t$")
    ax.plot(x_cut, elem_cut, color="#d07a00", lw=1.5, ls="--", label=r"阵元增益 $G_e$")
    ax.axhline(COMPOSITE_PEAK_DBI,
               color="#666666",
               lw=1.0,
               ls=":",
               label=f"最大复合增益 {COMPOSITE_PEAK_DBI:.1f} dBi")
    ax.set_title("（a）固定方位平面下的增益切面")
    ax.set_xlabel(r"离轴角偏移 $\Delta\vartheta$ / (°)")
    ax.set_ylabel("天线增益 / dBi")
    ax.set_xlim(-30.0, 30.0)
    ax.set_ylim(DISPLAY_FLOOR_DBI, 45.0)
    ax.set_xticks(np.arange(-30, 31, 10))
    ax.grid(True, color="#d0d0d0", lw=0.7, alpha=0.75)
    ax.legend(loc="upper right", frameon=True)

    ax = axes[1]
    im = ax.imshow(
        heat_display,
        origin="lower",
        extent=[az.min(), az.max(), el.min(), el.max()],
        cmap="viridis",
        vmin=DISPLAY_FLOOR_DBI,
        vmax=42.0,
        aspect="equal",
    )
    ax.contour(az_grid,
               el_grid,
               heat,
               levels=[COMPOSITE_PEAK_DBI - 3.0],
               colors="white",
               linewidths=1.0)
    ax.scatter([0.0],
               [0.0],
               s=240,
               facecolors="none",
               edgecolors="white",
               linewidths=1.4,
               zorder=4)
    ax.annotate("波束视轴",
                xy=(0.0, 0.0),
                xytext=(4.0, 5.0),
                color="white",
                fontsize=10.0,
                arrowprops={"arrowstyle": "->", "color": "white", "lw": 1.0})
    ax.set_title("（b）方位角与天顶角偏移下的复合增益")
    ax.set_xlabel(r"方位角偏移 $\Delta\varphi$ / (°)")
    ax.set_ylabel(r"天顶角偏移 $\Delta\vartheta$ / (°)")
    cbar = fig.colorbar(im, ax=ax, shrink=0.88, pad=0.025)
    cbar.set_label(r"复合增益 $G_t$ / dBi")

    fig.text(
        0.5,
        0.026,
        (
            "图 3-2 卫星侧相控阵天线复合增益方向图\n"
            "注：卫星侧阵列为 12×12 均匀平面阵列，阵元间距为 0.5λ，阵元峰值增益为 20 dBi，"
            "方向图宽度参数为 4°，最大衰减为 30 dB，波束指向地面固定锚点。"
        ),
        ha="center",
        va="bottom",
        fontsize=10.6,
        color="#111111",
    )
    fig.tight_layout(rect=[0.02, 0.125, 0.995, 0.98])

    fig.savefig(OUT_DIR / "satellite_composite_antenna_gain.png")
    fig.savefig(OUT_DIR / "satellite_composite_antenna_gain.pdf")


if __name__ == "__main__":
    write_cut_csv()
    plot()

#!/usr/bin/env python3
import argparse
import csv
import math
import os
import sys
from typing import Optional

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import patches
from matplotlib import transforms


def _set_csv_field_limit() -> None:
    limit = sys.maxsize
    while True:
        try:
            csv.field_size_limit(limit)
            return
        except OverflowError:
            limit //= 2


def _clean_csv_lines(handle):
    for line in handle:
        yield line.replace("\0", "")


def safe_float(value: str, default: float = math.nan) -> float:
    try:
        return float(value)
    except Exception:
        return default


def safe_int(value: str, default: int = 0) -> int:
    try:
        return int(value)
    except Exception:
        return default


def load_throughput_trace(path: str):
    rows = []
    with open(path, "r", encoding="utf-8") as handle:
        reader = csv.DictReader(_clean_csv_lines(handle))
        if not reader.fieldnames:
            raise RuntimeError(f"empty or invalid csv: {path}")

        for row in reader:
            rows.append(
                {
                    "time_s": safe_float(row.get("time_s", "")),
                    "ue": safe_int(row.get("ue", "")),
                    "serving_cell": safe_int(row.get("serving_cell", "")),
                    "serving_sat": safe_int(row.get("serving_sat", ""), -1),
                    "throughput_mbps": safe_float(row.get("throughput_mbps", ""), 0.0),
                    "in_handover": safe_int(row.get("in_handover", "")) == 1,
                    "active_ho_id": safe_int(row.get("active_ho_id", "")),
                }
            )

    rows.sort(key=lambda item: (item["ue"], item["time_s"]))
    return rows


def load_handover_events(path: str):
    rows = []
    with open(path, "r", encoding="utf-8") as handle:
        reader = csv.DictReader(_clean_csv_lines(handle))
        if not reader.fieldnames:
            raise RuntimeError(f"empty or invalid csv: {path}")

        for row in reader:
            rows.append(
                {
                    "time_s": safe_float(row.get("time_s", "")),
                    "ue": safe_int(row.get("ue", "")),
                    "ho_id": safe_int(row.get("ho_id", "")),
                    "event": row.get("event", "").strip(),
                    "source_cell": safe_int(row.get("source_cell", "")),
                    "target_cell": safe_int(row.get("target_cell", "")),
                    "source_sat": safe_int(row.get("source_sat", ""), -1),
                    "target_sat": safe_int(row.get("target_sat", ""), -1),
                    "delay_ms": safe_float(row.get("delay_ms", "")),
                    "ping_pong_detected": safe_int(row.get("ping_pong_detected", "")) == 1,
                }
            )

    rows.sort(key=lambda item: (item["ue"], item["ho_id"], item["time_s"]))
    return rows


def select_handover(events, ue: int, ho_id: Optional[int]):
    ue_events = [event for event in events if event["ue"] == ue]
    if not ue_events:
        raise RuntimeError(f"no handover events found for ue={ue}")

    starts = {event["ho_id"]: event for event in ue_events if event["event"] == "HO_START"}
    ends = {event["ho_id"]: event for event in ue_events if event["event"] == "HO_END_OK"}
    completed_ids = sorted(set(starts.keys()) & set(ends.keys()))
    if not completed_ids:
        raise RuntimeError(f"ue={ue} has no completed HO_START/HO_END_OK pair")

    selected_id = ho_id if ho_id is not None else completed_ids[0]
    if selected_id not in starts or selected_id not in ends:
        raise RuntimeError(f"handover ho_id={selected_id} not found for ue={ue}")

    return starts[selected_id], ends[selected_id]


def sat_label(sat_idx: int) -> str:
    return f"Sat-{sat_idx}" if sat_idx >= 0 else "Sat-?"


def estimate_sample_interval_seconds(times):
    if len(times) < 2:
        return math.nan
    deltas = [times[idx] - times[idx - 1] for idx in range(1, len(times)) if times[idx] > times[idx - 1]]
    if not deltas:
        return math.nan
    deltas.sort()
    return deltas[len(deltas) // 2]


def moving_average(values, window_size: int):
    if window_size <= 1:
        return list(values)

    out = []
    for idx in range(len(values)):
        start = max(0, idx - window_size + 1)
        chunk = values[start : idx + 1]
        out.append(sum(chunk) / float(len(chunk)))
    return out


def mean_or_nan(values):
    finite = [value for value in values if math.isfinite(value)]
    if not finite:
        return math.nan
    return sum(finite) / float(len(finite))


def add_marker_label(ax, x, y, text, color, x_offset_points, y_offset_points, ha):
    ax.annotate(
        text,
        xy=(x, y),
        xytext=(x_offset_points, y_offset_points),
        textcoords="offset points",
        ha=ha,
        va="top",
        fontsize=10.5,
        color=color,
        bbox={
            "boxstyle": "round,pad=0.18",
            "facecolor": "white",
            "edgecolor": "none",
            "alpha": 0.82,
        },
        zorder=6,
        clip_on=False,
    )


def plot_handover_window(
    samples,
    start_event,
    end_event,
    output_path: str,
    window_before: float,
    window_after: float,
    smooth_window_ms: float,
    show_raw: bool,
):
    ho_start = start_event["time_s"]
    ho_end = end_event["time_s"]
    window_start = ho_start - window_before
    window_end = ho_end + window_after

    window_samples = [
        sample
        for sample in samples
        if window_start <= sample["time_s"] <= window_end
    ]
    if not window_samples:
        raise RuntimeError("no throughput samples fall inside the requested handover window")

    times = [sample["time_s"] for sample in window_samples]
    raw_throughput = [sample["throughput_mbps"] for sample in window_samples]
    sample_interval_seconds = estimate_sample_interval_seconds(times)
    smooth_window_samples = 1
    if math.isfinite(sample_interval_seconds) and sample_interval_seconds > 0.0 and smooth_window_ms > 0.0:
        smooth_window_samples = max(1, int(round((smooth_window_ms / 1000.0) / sample_interval_seconds)))
    smooth_throughput = moving_average(raw_throughput, smooth_window_samples)

    pre_values = [sample["throughput_mbps"] for sample in window_samples if sample["time_s"] < ho_start]
    in_values = [
        sample["throughput_mbps"]
        for sample in window_samples
        if ho_start <= sample["time_s"] <= ho_end + 1e-12
    ]
    post_values = [sample["throughput_mbps"] for sample in window_samples if sample["time_s"] > ho_end]
    pre_avg = mean_or_nan(pre_values)
    in_avg = mean_or_nan(in_values)
    post_avg = mean_or_nan(post_values)

    ymax = max(max(raw_throughput), max(smooth_throughput), 1.0) * 1.20

    plt.rcParams.update(
        {
            "axes.spines.top": False,
            "axes.spines.right": False,
            "axes.facecolor": "#fbfbf8",
            "figure.facecolor": "white",
            "axes.labelsize": 12,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
            "font.size": 11,
        }
    )
    fig, ax = plt.subplots(figsize=(11.5, 6.2))
    ax.axvspan(ho_start, ho_end, color="#f4c7c3", alpha=0.45, zorder=0)
    if show_raw:
        ax.plot(times, raw_throughput, color="#adc6df", linewidth=1.4, alpha=0.9, zorder=2)
    ax.plot(times, smooth_throughput, color="#134a84", linewidth=2.8, zorder=3)
    ax.axvline(ho_start, color="#c62828", linestyle="--", linewidth=2.0, zorder=4)
    ax.axvline(ho_end, color="#c62828", linestyle="--", linewidth=2.0, zorder=4)

    if math.isfinite(pre_avg):
        ax.hlines(pre_avg, window_start, ho_start, colors="#4e7ca6", linestyles=":", linewidth=1.6, zorder=1)
    if math.isfinite(post_avg):
        ax.hlines(post_avg, ho_end, window_end, colors="#2f6f68", linestyles=":", linewidth=1.6, zorder=1)

    label_anchor_y = ymax * 0.985
    markers_are_close = (ho_end - ho_start) <= max(0.02, (window_end - window_start) * 0.03)
    if markers_are_close:
        add_marker_label(ax, ho_start, label_anchor_y, "HO Start", "#7a1616", -8, -4, "right")
        add_marker_label(ax, ho_end, label_anchor_y, "HO Success", "#7a1616", 8, -18, "left")
    else:
        add_marker_label(ax, ho_start, label_anchor_y, "HO Start", "#7a1616", 4, -4, "left")
        add_marker_label(ax, ho_end, label_anchor_y, "HO Success", "#7a1616", 4, -18, "left")

    band_transform = transforms.blended_transform_factory(ax.transData, ax.transAxes)
    band_y = 1.01
    band_h = 0.08
    source_color = "#1565d8"
    target_color = "#17b8d8"
    ho_color = "#f2d6d3"

    top_spans = [
        (window_start, ho_start, source_color, sat_label(start_event["source_sat"])),
        (ho_start, ho_end, ho_color, "HO"),
        (ho_end, window_end, target_color, sat_label(end_event["target_sat"])),
    ]
    for x0, x1, color, label in top_spans:
        width = max(0.0, x1 - x0)
        rect = patches.Rectangle(
            (x0, band_y),
            width,
            band_h,
            transform=band_transform,
            clip_on=False,
            linewidth=1.0,
            edgecolor="#0b2f4a",
            facecolor=color,
            alpha=0.95,
        )
        ax.add_patch(rect)
        if width > 0.0:
            ax.text(
                x0 + width / 2.0,
                band_y + band_h / 2.0,
                label,
                transform=band_transform,
                ha="center",
                va="center",
                fontsize=11,
                color="white" if color != ho_color else "#3c2f2f",
                fontweight="bold",
            )

    ax.set_xlim(window_start, window_end)
    ax.set_ylim(0.0, ymax)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Throughput (Mbps)")
    fig.suptitle(
        f"UE {start_event['ue']} Throughput Around Handover  |  "
        f"{sat_label(start_event['source_sat'])} -> {sat_label(end_event['target_sat'])}  |  "
        f"delay={end_event['delay_ms']:.1f} ms",
        fontsize=14,
        y=0.975,
    )
    ax.grid(True, axis="y", alpha=0.22, linewidth=0.8)
    ax.grid(False, axis="x")

    stats_lines = []
    if math.isfinite(pre_avg):
        stats_lines.append(f"Pre-HO avg: {pre_avg:.2f} Mbps")
    if math.isfinite(in_avg):
        stats_lines.append(f"HO-window avg: {in_avg:.2f} Mbps")
    if math.isfinite(post_avg):
        stats_lines.append(f"Post-HO avg: {post_avg:.2f} Mbps")
    if smooth_window_samples > 1 and math.isfinite(sample_interval_seconds):
        stats_lines.append(f"Smoothing: {smooth_window_samples} samples ({smooth_window_ms:.0f} ms)")
    if stats_lines:
        ax.text(
            0.015,
            0.96,
            "\n".join(stats_lines),
            transform=ax.transAxes,
            ha="left",
            va="top",
            fontsize=10.5,
            color="#27323a",
            bbox={"boxstyle": "round,pad=0.35", "facecolor": "white", "edgecolor": "#d9d9d9", "alpha": 0.95},
        )

    if end_event["ping_pong_detected"]:
        ax.text(
            0.98,
            0.02,
            "Ping-pong detected",
            transform=ax.transAxes,
            ha="right",
            va="bottom",
            fontsize=10,
            color="#8b1e1e",
        )

    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.90))
    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    fig.savefig(output_path, dpi=180, bbox_inches="tight")
    plt.close(fig)


def main():
    _set_csv_field_limit()

    parser = argparse.ArgumentParser(
        description="Plot per-UE throughput around a completed handover window"
    )
    parser.add_argument(
        "--throughput-trace",
        default="scratch/results/handover_dl_throughput_trace.csv",
        help="Input handover throughput trace CSV",
    )
    parser.add_argument(
        "--event-trace",
        default="scratch/results/handover_event_trace.csv",
        help="Input handover event trace CSV",
    )
    parser.add_argument("--ue", type=int, required=True, help="UE index to plot")
    parser.add_argument(
        "--ho-id",
        type=int,
        default=None,
        help="Specific HO id to plot; default is the first completed handover for the UE",
    )
    parser.add_argument(
        "--window-before",
        type=float,
        default=1.0,
        help="Seconds to keep before HO Start",
    )
    parser.add_argument(
        "--window-after",
        type=float,
        default=1.0,
        help="Seconds to keep after HO Success",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output PNG path; default is scratch/results/handover_throughput_ueX_hoY.png",
    )
    parser.add_argument(
        "--smooth-window-ms",
        type=float,
        default=20.0,
        help="Trailing moving-average window in milliseconds; 0 disables smoothing",
    )
    parser.add_argument(
        "--show-raw",
        action="store_true",
        help="Also draw the raw unsmoothed throughput samples behind the main curve",
    )
    args = parser.parse_args()

    if args.window_before < 0.0 or args.window_after < 0.0 or args.smooth_window_ms < 0.0:
        raise RuntimeError("window-before, window-after and smooth-window-ms must be >= 0")

    throughput_samples = load_throughput_trace(args.throughput_trace)
    handover_events = load_handover_events(args.event_trace)
    start_event, end_event = select_handover(handover_events, args.ue, args.ho_id)
    ue_samples = [sample for sample in throughput_samples if sample["ue"] == args.ue]
    if not ue_samples:
        raise RuntimeError(f"no throughput samples found for ue={args.ue}")

    output_path = args.output
    if not output_path:
        output_path = os.path.join(
            "scratch",
            "results",
            f"handover_throughput_ue{args.ue}_ho{start_event['ho_id']}.png",
        )

    plot_handover_window(
        ue_samples,
        start_event,
        end_event,
        output_path,
        args.window_before,
        args.window_after,
        args.smooth_window_ms,
        args.show_raw,
    )

    print("=== Handover Throughput Plot ===")
    print(f"[Plot] ue={args.ue} ho_id={start_event['ho_id']}")
    print(
        f"[Plot] start={start_event['time_s']:.3f}s end={end_event['time_s']:.3f}s "
        f"delay={end_event['delay_ms']:.3f}ms"
    )
    print(
        f"[Plot] path={sat_label(start_event['source_sat'])} -> {sat_label(end_event['target_sat'])}"
    )
    print(f"[Plot] output={output_path}")


if __name__ == "__main__":
    main()

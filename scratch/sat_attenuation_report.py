#!/usr/bin/env python3
import argparse
import csv
import math
import os


def safe_float(v: str) -> float:
    try:
        return float(v)
    except Exception:
        return math.nan


def parse_trace(path: str):
    records = []
    with open(path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise RuntimeError(f"empty or invalid csv: {path}")

        for row in reader:
            time_s = safe_float(row["time_s"])
            ue = int(row["ue"]) if "ue" in row and row["ue"] != "" else 0
            sat = int(row["sat"])
            cell = int(row["cell"])
            beam_locked = int(row["beam_locked"]) == 1
            scan_loss = safe_float(row["scan_loss_db"])
            pattern_loss = safe_float(row["pattern_loss_db"])
            fspl = safe_float(row["fspl_db"])
            atm = safe_float(row["atm_loss_db"])
            rsrp = safe_float(row["rsrp_dbm"])

            total_loss = math.nan
            if beam_locked and all(math.isfinite(x) for x in [scan_loss, pattern_loss, fspl, atm]):
                total_loss = scan_loss + pattern_loss + fspl + atm

            records.append(
                {
                    "time_s": time_s,
                    "ue": ue,
                    "sat": sat,
                    "cell_id": cell,
                    "beam_locked": 1 if beam_locked else 0,
                    "scan_loss_db": scan_loss,
                    "pattern_loss_db": pattern_loss,
                    "fspl_db": fspl,
                    "atm_loss_db": atm,
                    "total_loss_db": total_loss,
                    "rsrp_dbm": rsrp,
                }
            )

    if not records:
        raise RuntimeError(f"no valid rows parsed from: {path}")
    records.sort(key=lambda x: (x["time_s"], x["ue"], x["sat"]))
    return records


def write_per_time(path: str, records):
    fields = [
        "time_s",
        "ue",
        "sat",
        "cell_id",
        "beam_locked",
        "scan_loss_db",
        "pattern_loss_db",
        "fspl_db",
        "atm_loss_db",
        "total_loss_db",
        "rsrp_dbm",
    ]
    output_dir = os.path.dirname(path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for r in records:
            writer.writerow(r)


def main():
    parser = argparse.ArgumentParser(description="Per-time per-satellite attenuation export")
    parser.add_argument("--input", default="scratch/results/sat_beam_trace.csv", help="Input CSV path")
    parser.add_argument(
        "--per-time-out",
        default="scratch/results/sat_attenuation_per_time.csv",
        help="Per-time detail CSV output path",
    )
    args = parser.parse_args()

    records = parse_trace(args.input)
    write_per_time(args.per_time_out, records)

    t_min = min(r["time_s"] for r in records)
    t_max = max(r["time_s"] for r in records)
    ues = sorted({r["ue"] for r in records})
    sats = sorted({r["sat"] for r in records})
    print("=== Satellite Attenuation Per-Time Export ===")
    print(f"[AttenuationScript] ue count: {len(ues)} (ue ids={ues})")
    print(f"[AttenuationScript] sat count: {len(sats)} (sat ids={sats})")
    print(f"[AttenuationScript] time range: {t_min:.3f}s -> {t_max:.3f}s")
    print(f"[AttenuationScript] rows: {len(records)}")
    print(f"[AttenuationScript] per-time exported: {args.per_time_out}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
import argparse
import csv
import math
import os

OUTPUT_COLUMNS = [
    ("time_s", "时间_秒"),
    ("ue", "用户序号"),
    ("sat", "卫星序号"),
    ("cell_id", "小区编号"),
    ("beam_locked", "波束是否锁定"),
    ("rsrp_dbm", "参考信号接收功率_dBm"),
    ("total_loss_db", "总损耗_dB"),
    ("attached_ue_count", "已接入UE数"),
    ("load_score", "负载评分"),
    ("admission_allowed", "是否允许接纳切入"),
]


def safe_float(value: str) -> float:
    try:
        return float(value)
    except Exception:
        return math.nan


def safe_int(value: str, default: int = 0) -> int:
    try:
        return int(value)
    except Exception:
        return default


def parse_trace(path: str):
    records = []
    with open(path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise RuntimeError(f"empty or invalid csv: {path}")

        for row in reader:
            beam_locked = safe_int(row.get("beam_locked", "0")) == 1
            scan_loss_db = safe_float(row.get("scan_loss_db", ""))
            pattern_loss_db = safe_float(row.get("pattern_loss_db", ""))
            fspl_db = safe_float(row.get("fspl_db", ""))
            atm_loss_db = safe_float(row.get("atm_loss_db", ""))

            total_loss_db = math.nan
            if beam_locked and all(
                math.isfinite(x) for x in [scan_loss_db, pattern_loss_db, fspl_db, atm_loss_db]
            ):
                total_loss_db = scan_loss_db + pattern_loss_db + fspl_db + atm_loss_db

            records.append(
                {
                    "time_s": safe_float(row.get("time_s", "")),
                    "ue": safe_int(row.get("ue", "0")),
                    "sat": safe_int(row.get("sat", "0")),
                    "cell_id": safe_int(row.get("cell", "0")),
                    "beam_locked": 1 if beam_locked else 0,
                    "rsrp_dbm": safe_float(row.get("rsrp_dbm", "")),
                    "total_loss_db": total_loss_db,
                    "attached_ue_count": safe_int(row.get("attached_ue_count", "0")),
                    "offered_packet_rate": safe_float(row.get("offered_packet_rate", "")),
                    "load_score": safe_float(row.get("load_score", "")),
                    "admission_allowed": safe_int(row.get("admission_allowed", "0")),
                }
            )

    if not records:
        raise RuntimeError(f"no valid rows parsed from: {path}")

    records.sort(key=lambda x: (x["time_s"], x["ue"], x["sat"]))
    return records


def write_per_time(path: str, records):
    fields = [header for _, header in OUTPUT_COLUMNS]
    output_dir = os.path.dirname(path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for record in records:
            writer.writerow({header: record[key] for key, header in OUTPUT_COLUMNS})


def main():
    parser = argparse.ArgumentParser(description="Export compact per-time beam and load report")
    parser.add_argument("--input", default="scratch/results/sat_beam_trace.csv", help="Input CSV path")
    parser.add_argument(
        "--per-time-out",
        default="scratch/results/sat_attenuation_per_time.csv",
        help="Per-time compact CSV output path",
    )
    args = parser.parse_args()

    records = parse_trace(args.input)
    write_per_time(args.per_time_out, records)

    t_min = min(r["time_s"] for r in records)
    t_max = max(r["time_s"] for r in records)
    ues = sorted({r["ue"] for r in records})
    sats = sorted({r["sat"] for r in records})
    print("=== Satellite Beam/Load Per-Time Export ===")
    print(f"[Report] ue count: {len(ues)} (ue ids={ues})")
    print(f"[Report] sat count: {len(sats)} (sat ids={sats})")
    print(f"[Report] time range: {t_min:.3f}s -> {t_max:.3f}s")
    print(f"[Report] rows: {len(records)}")
    print(f"[Report] per-time exported: {args.per_time_out}")


if __name__ == "__main__":
    main()

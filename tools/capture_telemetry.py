#!/usr/bin/env python3
"""Capture a Zopapa telemetry dump ('Z' command) over serial and plot it.

Phase 0 (PLAN-speed-scheduling-v3_1.md). Deliberately dumb: connect, send
'Z', read until the "--- END ---" marker, save the raw dump and a CSV to
logs/, plot position (and correction) vs. time.

Usage:
    python3 tools/capture_telemetry.py /dev/ttyUSB0 [--baud 9600] [--no-plot]

The robot must already be running (state != Detenido) when you send 'Z',
since the ring buffer only fills while correr()/autotuneRelay() are active.
This script sends 'Z' itself right after connecting — start the robot, get
it doing whatever you want measured, then run this script.
"""
import argparse
import csv
import re
import sys
import time
from pathlib import Path

import serial

REPO_ROOT = Path(__file__).resolve().parent.parent
LOGS_DIR = REPO_ROOT / "logs"


def capture(port, baud, timeout_s):
    with serial.Serial(port, baud, timeout=1) as ser:
        time.sleep(2)  # let the board finish any reset-on-open before we talk to it
        ser.reset_input_buffer()
        ser.write(b"Z\n")

        lines = []
        deadline = time.time() + timeout_s
        started = False
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if "--- TELEMETRY DUMP ---" in line:
                started = True
            if started:
                lines.append(line)
            if "--- END ---" in line:
                break
        else:
            raise TimeoutError(
                f"No '--- END ---' marker within {timeout_s}s — is the robot "
                "running and the ring buffer non-empty?"
            )
        return lines


def parse(lines):
    meta = {}
    rows = []
    header_seen = False
    for line in lines:
        m = re.match(r"KP=([\-\d.]+) KD=([\-\d.]+) SPEED=(\d+)", line)
        if m:
            meta["KP"], meta["KD"], meta["SPEED"] = m.groups()
            continue
        m = re.match(r"loop_dt_us min=(\d+) max=(\d+) mean=([\-\d.]+)", line)
        if m:
            meta["dt_min_us"], meta["dt_max_us"], meta["dt_mean_us"] = m.groups()
            continue
        m = re.match(r"samples=(\d+) decimation=(\d+)", line)
        if m:
            meta["samples"], meta["decimation"] = m.groups()
            continue
        if line == "dt_us,posicion,corr":
            header_seen = True
            continue
        if header_seen and "," in line:
            parts = line.split(",")
            if len(parts) == 3:
                try:
                    rows.append(tuple(int(p) for p in parts))
                except ValueError:
                    pass
    return meta, rows


def save(meta, rows, stem):
    LOGS_DIR.mkdir(exist_ok=True)
    csv_path = LOGS_DIR / f"{stem}.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_ms", "dt_us", "posicion", "corr"])
        t_ms = 0.0
        for dt_us, posicion, corr in rows:
            t_ms += dt_us / 1000.0
            w.writerow([f"{t_ms:.3f}", dt_us, posicion, corr])
    return csv_path


def plot(rows, meta, stem):
    import matplotlib.pyplot as plt

    t_ms, position, corr = [], [], []
    acc = 0.0
    for dt_us, p, c in rows:
        acc += dt_us / 1000.0
        t_ms.append(acc)
        position.append(p)
        corr.append(c)

    fig, ax1 = plt.subplots(figsize=(10, 5))
    ax1.plot(t_ms, position, label="posicion", color="tab:blue")
    ax1.set_xlabel("t (ms)")
    ax1.set_ylabel("posicion", color="tab:blue")
    ax1.axhline(0, color="gray", linewidth=0.5)

    ax2 = ax1.twinx()
    ax2.plot(t_ms, corr, label="corr", color="tab:orange", alpha=0.6)
    ax2.set_ylabel("corr", color="tab:orange")

    title = f"SPEED={meta.get('SPEED', '?')} KP={meta.get('KP', '?')} KD={meta.get('KD', '?')}"
    if "decimation" in meta:
        title += f"  (decimation={meta['decimation']}, {meta.get('samples', '?')} samples)"
    ax1.set_title(title)
    fig.tight_layout()

    png_path = LOGS_DIR / f"{stem}.png"
    fig.savefig(png_path)
    print(f"Plot saved to {png_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="Serial port, e.g. /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--timeout", type=float, default=15.0, help="Seconds to wait for the dump to finish")
    ap.add_argument("--no-plot", action="store_true", help="Skip matplotlib (just save CSV)")
    ap.add_argument("--tag", default="capture", help="Filename prefix under logs/")
    args = ap.parse_args()

    lines = capture(args.port, args.baud, args.timeout)
    meta, rows = parse(lines)
    if not rows:
        print("No data rows parsed — raw dump was:", file=sys.stderr)
        print("\n".join(lines), file=sys.stderr)
        sys.exit(1)

    stem = f"{args.tag}_{time.strftime('%Y%m%d_%H%M%S')}"
    raw_path = LOGS_DIR / f"{stem}.raw.txt"
    LOGS_DIR.mkdir(exist_ok=True)
    raw_path.write_text("\n".join(lines) + "\n")

    csv_path = save(meta, rows, stem)
    print(f"Meta: {meta}")
    print(f"{len(rows)} samples -> {csv_path}")

    if not args.no_plot:
        try:
            plot(rows, meta, stem)
        except ImportError:
            print("matplotlib not installed (pip install matplotlib) — CSV saved, skipping plot.")


if __name__ == "__main__":
    main()

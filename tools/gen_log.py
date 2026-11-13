#!/usr/bin/env python3
"""Generate a synthetic device log for benchmarking LogTriage.

Usage: python3 tools/gen_log.py LINES [--corrupt-every N] [--seed S] > big.log

About 1 in --corrupt-every lines is deliberately damaged (default: 500) so the
parser's error handling is exercised as well as the happy path.
"""
import argparse
import random

MESSAGES = [
    ("INFO", "app", "telemetry batch uploaded ({n} records)"),
    ("INFO", "net", "eth0: link up, 100Mbps full duplex"),
    ("DEBUG", "sensor", "sensor {n} sample ok"),
    ("WARN", "net", "packet loss {n}% on eth0"),
    ("WARN", "thermal", "CPU temperature {n}C, throttling"),
    ("WARN", "app", "heap usage at {n}%"),
    ("ERROR", "net", "connection timed out to 10.0.0.{n}:8883"),
    ("ERROR", "mmc", "I/O error, dev mmcblk0, sector {n}"),
    ("ERROR", "sensor", "i2c bus error on sensor {n}"),
    ("ERROR", "kernel", "Out of memory: killed process {n}"),
    ("ERROR", "power", "battery undervoltage detected: 3.{n}V"),
    ("FATAL", "kernel", "Kernel panic - not syncing: watchdog timeout"),
]
WEIGHTS = [30, 10, 20, 10, 4, 6, 8, 4, 4, 2, 1, 1]


def corrupt(line: str, rng: random.Random) -> str:
    kind = rng.randrange(4)
    if kind == 0:
        return line[: rng.randrange(5, len(line))]  # truncated
    if kind == 1:
        return "\x01\x02garbage\x07" + line[10:]  # binary noise
    if kind == 2:
        return line.replace("[", "", 1)  # broken brackets
    return "random text with no structure at all"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("lines", type=int)
    ap.add_argument("--corrupt-every", type=int, default=500)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    out = []
    for i in range(args.lines):
        secs = i % 86400
        ts = "2026-11-%02d %02d:%02d:%02d" % (
            1 + (i // 86400) % 28, secs // 3600, (secs // 60) % 60, secs % 60)
        level, module, template = rng.choices(MESSAGES, WEIGHTS)[0]
        line = "%s [%s] [%s] %s" % (ts, level, module, template.format(n=rng.randrange(1, 100)))
        if args.corrupt_every and rng.randrange(args.corrupt_every) == 0:
            line = corrupt(line, rng)
        out.append(line)
        if len(out) >= 10000:
            print("\n".join(out))
            out = []
    if out:
        print("\n".join(out))


if __name__ == "__main__":
    main()

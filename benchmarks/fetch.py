from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import platform
import re
import statistics
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--label", required=True)
parser.add_argument("--revision", required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--sizes", type=int, nargs="+", default=[512, 2048, 8192])
parser.add_argument("--runs", type=int, default=5)
args = parser.parse_args()
if args.runs < 3 or any(n < 1 for n in args.sizes):
    parser.error("use at least three runs and positive workload sizes")
if args.output.exists():
    parser.error(f"output already exists: {args.output}")

compiler = os.environ.get("CC", "cc")
flags = ["-O2", "-DNDEBUG", "-Wall", "-Wextra", "-Werror"]
trace_range = "T2147483647:2147483647"
source_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in sorted([*root.glob("*.c"), *root.glob("*.h")])}
report = {
    "label": args.label,
    "source_revision": args.revision,
    "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "platform": platform.platform(),
    "machine": platform.machine(),
    "compiler": subprocess.check_output([compiler, "--version"], text=True).splitlines()[0],
    "flags": flags,
    "source_sha256": source_hashes,
    "timing": "process startup, input validation, simulation, and summary output; build excluded",
    "trace_range": trace_range,
    "warmup_runs": 1,
    "results": [],
}

def workload(count):
    offset = 144
    jump = (((offset >> 20) & 1) << 31 | ((offset >> 1) & 0x3ff) << 21
            | ((offset >> 11) & 1) << 20 | ((offset >> 12) & 0xff) << 12 | 0x6f)
    words = [jump] + [0x13] * 25 + [0] * 10
    words += [0x00128293] * count + [0x8067] + [0x13] * 8
    return "".join(f"{word:032b}\n" for word in words)

def validate(summary, count):
    registers = summary.split("Integer Registers:\n", 1)[1].split("Data Memory:", 1)[0]
    pairs = [(int(r), int(v)) for r, v in re.findall(r"R(\d+)\s+(-?\d+)", registers)]
    expected = [(r, count if r == 5 else 0) for r in range(32)]
    if pairs != expected:
        raise SystemExit(f"incorrect registers for {count} increments: {pairs}")
    memory = summary.split("Data Memory:\n", 1)[1].split("\n\n", 1)[0]
    if memory != "\n".join(f"{600 + 4*i}: 0" for i in range(10)):
        raise SystemExit(f"incorrect memory for {count} increments")

with tempfile.TemporaryDirectory(prefix="riscv-fetch-") as directory:
    work = Path(directory)
    binary = work / "sim"
    subprocess.run([compiler, *flags, "-o", str(binary),
                    *[str(root / name) for name in
                      ("main.c", "disassembler.c", "pipeline.c", "utilities.c")]], check=True)
    for count in args.sizes:
        source, output = work / "input.txt", work / "output.txt"
        content = workload(count)
        source.write_text(content)
        times = []
        reference = None
        for run in range(args.runs + 1):
            start = time.perf_counter()
            subprocess.run([str(binary), str(source), str(output), "sim", trace_range],
                           check=True, timeout=120)
            elapsed = time.perf_counter() - start
            summary = output.read_text()
            validate(summary, count)
            if reference is not None and summary != reference:
                raise SystemExit(f"summary changed between runs for {count} increments")
            reference = summary
            if run:
                times.append(elapsed)
        result = {
            "increments": count,
            "input_bytes": len(content.encode()),
            "input_sha256": hashlib.sha256(content.encode()).hexdigest(),
            "seconds": times,
            "median_seconds": statistics.median(times),
            "min_seconds": min(times),
            "max_seconds": max(times),
            "summary": reference,
        }
        report["results"].append(result)
        print(f"{count:5d} increments: median={result['median_seconds']:.6f}s "
              f"range={min(times):.6f}-{max(times):.6f}s", flush=True)

args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open("x") as output:
    json.dump(report, output, indent=2)
    output.write("\n")
print(f"Saved {args.output}")

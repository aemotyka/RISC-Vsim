"""Fibonacci regression."""

from pathlib import Path
import hashlib
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = root / "tests/fixtures/fibonacci.txt"
expected_sha256 = "298ceecba1ee295e96d5385ee03f4f03defe2765792fd50f26b86077811ed40c"

with tempfile.TemporaryDirectory(prefix="riscv-regression-") as directory:
    work = Path(directory)
    binary = work / "RISC-Vsim"
    subprocess.run([
        "cc", "-Wall", "-Wextra", "-Werror", "-g",
        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-o", str(binary),
        *[str(root / name) for name in
          ("main.c", "disassembler.c", "pipeline.c", "utilities.c")],
    ], check=True)
    print("PASS: warning-free sanitizer build")

    for mode in ("dis", "sim"):
        subprocess.run(
            [str(binary), str(fixture), str(work / f"{mode}.out"), mode],
            check=True, timeout=5,
        )
        print(f"PASS: Fibonacci {mode}")

    output = (work / "sim.out").read_bytes()
    actual_sha256 = hashlib.sha256(output).hexdigest()
    if actual_sha256 != expected_sha256:
        raise SystemExit(f"Fibonacci trace mismatch: {actual_sha256}")
    print("PASS: complete Fibonacci trace (147 cycles)")

    trace, summary = output.decode().split("**** Summary", 1)
    summary_registers = summary.split("Integer Registers:\n", 1)[1].split("Data Memory:", 1)[0]
    trace_registers = trace.rsplit("Integer Registers:\n", 1)[1].split("Data Memory:", 1)[0]
    pairs = re.findall(r"R(\d+)\s+(-?\d+)", summary_registers)
    if [int(r) for r, _ in pairs] != list(range(32)):
        raise SystemExit("Summary must print R0 through R31 exactly once")
    if pairs != re.findall(r"R(\d+)\s+(-?\d+)", trace_registers):
        raise SystemExit("Summary registers differ from the final trace")
    print("PASS: all 32 summary registers match the final trace")
    memory = summary.split("Data Memory:\n", 1)[1].split("\n\n", 1)[0]
    expected = (1, 1, 2, 3, 5, 8, 13, 21, 34, 55)
    expected_memory = "\n".join(
        f"{600 + 4 * i}: {value}" for i, value in enumerate(expected)
    )
    if memory != expected_memory:
        raise SystemExit("Incorrect Fibonacci memory values")
    print("PASS: independently specified Fibonacci memory values")

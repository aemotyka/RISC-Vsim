"""Fibonacci regression."""

from pathlib import Path
import hashlib
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = root / "tests/fixtures/fibonacci.txt"
expected_sha256 = "ab7238908a8d3b47d96fcb1d82ca21b69e35a2f8cd290844928dae96e510654a"

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

    summary = output.decode().split("**** Summary", 1)[1]
    memory = summary.split("Data Memory:\n", 1)[1].split("\n\n", 1)[0]
    expected = (1, 1, 2, 3, 5, 8, 13, 21, 34, 55)
    expected_memory = "\n".join(
        f"{600 + 4 * i}: {value}" for i, value in enumerate(expected)
    )
    if memory != expected_memory:
        raise SystemExit("Incorrect Fibonacci memory values")
    print("PASS: independently specified Fibonacci memory values")

"""Historical regression using a reconstructed input; not an ISA conformance test.

The jump encoding and zero padding are inferred. The generated trace matches
the surviving output.txt, including known simulator defects.
"""

from pathlib import Path
import hashlib
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = root / "tests/fixtures/fibonacci.txt"
historical_sha256 = "426a95b3a464e4a16f36b97f6c27148cb7b64ba2761310e61d8a88e3b17fce4b"

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
    if actual_sha256 != historical_sha256:
        raise SystemExit(f"Historical trace mismatch: {actual_sha256}")
    print("PASS: complete trace matches historical output (147 cycles)")

    summary = output.decode().split("**** Summary", 1)[1]
    memory = summary.split("Data Memory:\n", 1)[1].split("\n\n", 1)[0]
    expected = (1, 1, 2, 3, 5, 8, 13, 21, 34, 55)
    expected_memory = "\n".join(
        f"{600 + 4 * i}: {value}" for i, value in enumerate(expected)
    )
    if memory != expected_memory:
        raise SystemExit("Incorrect Fibonacci memory values")
    print("PASS: independently specified Fibonacci memory values")

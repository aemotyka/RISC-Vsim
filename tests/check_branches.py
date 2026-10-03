from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def branch(rs1, rs2, funct3, offset):
    return (((offset >> 12) & 1) << 31
            | ((offset >> 5) & 0x3f) << 25
            | rs2 << 20 | rs1 << 15 | funct3 << 12
            | ((offset >> 1) & 0xf) << 8
            | ((offset >> 11) & 1) << 7 | 0x63)

cases = []
for name, funct3, a, b, taken in (
    ("beq-taken", 0, 1, 1, True), ("beq-not-taken", 0, 1, 2, False),
    ("bne-taken", 1, 1, 2, True), ("bne-not-taken", 1, 1, 1, False),
    ("blt-taken", 4, 1, 2, True), ("blt-not-taken", 4, 2, 1, False),
    ("bge-taken", 5, 2, 1, True), ("bge-not-taken", 5, 1, 2, False),
):
    for mode in ("settled", "forward-rs1", "forward-rs2"):
        words = [0x13] * 26 + [0] * 10
        setup = [(a << 20) | 0x93, (b << 20) | 0x113]
        words[:2] = setup[::-1] if mode == "forward-rs2" else setup
        index = 8 if mode == "settled" else 2
        words[index] = branch(1, 2, funct3, 32)
        words[index + 6] = 0x06300313
        words[index + 8] = 0x00700293
        words[index + 12] = 0x8067
        cases.append((f"{name}-{mode}", words, 0 if taken else 99))

words = [0x13] * 26 + [0] * 10
words[0] = 0x0200006f
words[1] = 0x00700293
words[2] = 0x0280006f
words[8] = branch(0, 0, 0, -28)
words[12] = 0x8067
cases.append(("backward", words, 0))

words = [0x13] * 528
words[26:36] = [0] * 10
words[0] = branch(0, 0, 0, 2048)
words[6] = 0x06300313
words[512] = 0x00700293
words[516] = 0x8067
cases.append(("positive-offset-2048", words, 0))

with tempfile.TemporaryDirectory(prefix="riscv-branches-") as directory:
    work = Path(directory)
    binary = work / "sim"
    subprocess.run([
        "cc", "-Wall", "-Wextra", "-Werror", "-g",
        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-o", str(binary),
        *[str(root / name) for name in
          ("main.c", "disassembler.c", "pipeline.c", "utilities.c")],
    ], check=True)
    for name, words, expected_r6 in cases:
        source, output = work / "input.txt", work / "output.txt"
        source.write_text("".join(f"{word:032b}\n" for word in words))
        subprocess.run([str(binary), str(source), str(output), "sim"],
                       check=True, timeout=5)
        trace = output.read_text().split("**** Summary", 1)[0]
        registers = trace.rsplit("Integer Registers:\n", 1)[1]
        registers = registers.split("Data Memory:", 1)[0]
        values = {int(r): int(v) for r, v in
                  re.findall(r"R(\d+)\s+(-?\d+)", registers)}
        if values[5] != 7 or values[6] != expected_r6:
            raise SystemExit(f"FAIL: {name}: R5={values[5]}, R6={values[6]}")
        print(f"PASS: {name}")

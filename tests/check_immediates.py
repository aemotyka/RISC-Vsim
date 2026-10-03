from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def addi(rd, rs1, imm):
    return (imm & 0xfff) << 20 | rs1 << 15 | rd << 7 | 0x13

def slti(rd, rs1, imm):
    return addi(rd, rs1, imm) | 2 << 12

def load(rd, rs1, imm):
    return (imm & 0xfff) << 20 | rs1 << 15 | 2 << 12 | rd << 7 | 3

def store(rs2, rs1, imm):
    imm &= 0xfff
    return (imm >> 5) << 25 | rs2 << 20 | rs1 << 15 | 2 << 12 | (imm & 31) << 7 | 0x23

with tempfile.TemporaryDirectory(prefix="riscv-immediates-") as directory:
    work = Path(directory)
    binary = work / "sim"
    subprocess.run([
        "cc", "-Wall", "-Wextra", "-Werror", "-g",
        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-o", str(binary),
        *[str(root / name) for name in
          ("main.c", "disassembler.c", "pipeline.c", "utilities.c")],
    ], check=True)
    for offset in (-1, -4, -2048):
        words = [0x13] * 26 + [0] * 10
        base = 600 - offset
        words[0] = addi(1, 0, min(base, 2047))
        words[1] = addi(1, 1, max(base - 2047, 0))
        words[2] = addi(2, 0, 45)
        words[3] = addi(2, 2, -3)
        words[4] = slti(3, 2, -1)
        words[5] = slti(4, 2, 43)
        words[6] = slti(6, 2, 42)
        words[7] = slti(7, 2, 41)
        words[8] = store(2, 1, offset)
        words[12] = load(5, 1, offset)
        words[20] = 0x8067
        source, output = work / "input.txt", work / "output.txt"
        source.write_text("".join(f"{word:032b}\n" for word in words))
        subprocess.run([str(binary), str(source), str(output), "sim"],
                       check=True, timeout=5)
        trace = output.read_text()
        if f"LW R5, {offset}(R1)" not in trace:
            raise SystemExit(f"FAIL: decoded load offset {offset}")
        registers = trace.split("**** Summary", 1)[0].rsplit("Integer Registers:\n", 1)[1]
        values = {int(r): int(v) for r, v in
                  re.findall(r"R(\d+)\s+(-?\d+)", registers.split("Data Memory:", 1)[0])}
        if values[2] != 42 or values[5] != 42:
            raise SystemExit(f"FAIL: offset {offset}: R2={values[2]}, R5={values[5]}")
        expected = {3: 0, 4: 1, 6: 0, 7: 0}
        if any(values[r] != value for r, value in expected.items()):
            raise SystemExit(f"FAIL: SLTI: { {r: values[r] for r in expected} }")
        subprocess.run([str(binary), str(source), str(output), "dis"],
                       check=True, timeout=5)
        disassembly = output.read_text()
        for instruction in ("ADDI x2, x2, -3", f"SW x2 {offset}(x1)", f"LW x5, {offset}(x1)"):
            if instruction not in disassembly:
                raise SystemExit(f"FAIL: missing {instruction}")
        print(f"PASS: negative immediate {offset}")

from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def addi(rd, rs1, imm):
    return (imm & 0xfff) << 20 | rs1 << 15 | rd << 7 | 0x13

def op(rd, rs1, rs2, funct3=0, funct7=0):
    return funct7 << 25 | rs2 << 20 | rs1 << 15 | funct3 << 12 | rd << 7 | 0x33

def slti(rd, rs1, imm):
    return addi(rd, rs1, imm) | 2 << 12

cases = [
    ("rs2-hazard-report", [addi(5, 0, 42), 0x13, op(6, 0, 5)], {5: 42, 6: 42}),
    ("wide-arithmetic", [addi(1, 0, 2047), addi(2, 0, 5),
        op(1, 1, 2, 1), addi(1, 1, 100), op(5, 1, 1)],
        {1: 65604, 5: 131208}),
    ("wrap-and-logical-shift", [addi(1, 0, -1), addi(2, 0, 1),
        addi(3, 0, 31), op(5, 1, 2), op(6, 0, 2, 0, 32),
        op(7, 1, 3, 1), op(8, 1, 3, 5)],
        {1: -1, 5: 0, 6: -1, 7: -2147483648, 8: 1}),
    ("masked-shifts", [addi(1, 0, -1), addi(2, 0, 1),
        addi(3, 0, 32), addi(4, 0, 33), op(5, 2, 3, 1),
        op(6, 1, 4, 5), addi(3, 0, 63), op(7, 2, 3, 1)],
        {5: 1, 6: 2147483647, 7: -2147483648}),
    ("signed-comparisons", [addi(1, 0, -1), addi(2, 0, 1),
        op(5, 1, 2, 2), op(6, 2, 1, 2), slti(7, 1, 0), slti(8, 1, -1)],
        {5: 1, 6: 0, 7: 1, 8: 0}),
]

words = [0x13] * 20
words[0:4] = [addi(1, 0, 600), addi(2, 0, 2047), addi(3, 0, 17), op(2, 2, 3, 1)]
words[6] = 2 << 20 | 1 << 15 | 2 << 12 | 0x23
words[7] = addi(4, 0, -1)
words[10] = 1 << 15 | 2 << 12 | 5 << 7 | 3
words[11] = 4 << 20 | 1 << 15 | 2 << 12 | 4 << 7 | 0x23
words[15] = 4 << 20 | 1 << 15 | 2 << 12 | 6 << 7 | 3
cases.append(("word-load-store", words, {2: 268304384, 5: 268304384, 6: -1}))

with tempfile.TemporaryDirectory(prefix="riscv-arithmetic-") as directory:
    work = Path(directory)
    binary = work / "sim"
    subprocess.run([
        "cc", "-Wall", "-Wextra", "-Werror", "-g",
        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-o", str(binary),
        *[str(root / name) for name in
          ("main.c", "disassembler.c", "pipeline.c", "utilities.c")],
    ], check=True)
    for name, program, expected in cases:
        words = program + [0x13] * (20 - len(program)) + [0x8067] + [0x13] * 5 + [0] * 10
        source, output = work / "input.txt", work / "output.txt"
        source.write_text("".join(f"{word:032b}\n" for word in words))
        subprocess.run([str(binary), str(source), str(output), "sim"],
                       check=True, timeout=5)
        trace = output.read_text().split("**** Summary", 1)[0]
        registers = trace.rsplit("Integer Registers:\n", 1)[1].split("Data Memory:", 1)[0]
        values = {int(r): int(v) for r, v in re.findall(r"R(\d+)\s+(-?\d+)", registers)}
        for register, value in expected.items():
            if values[register] != value:
                raise SystemExit(f"FAIL: {name}: R{register}={values[register]}, expected {value}")
        if name == "rs2-hazard-report":
            expected = "(ADDI R5, R0, #42) to (ADD R6, R0, R5)"
            cycle = next((c for c in trace.split("***** Cycle #")
                          if " * ID : ADD R6, R0, R5\n" in c
                          and " * EX : ADDI R5, R0, #42\n" in c), "")
            if " Detected: " + expected not in cycle:
                raise SystemExit("FAIL: missing rs2 hazard report")
        print(f"PASS: {name}")

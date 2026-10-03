from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def branch(rs1, rs2, funct3):
    return 1 << 25 | rs2 << 20 | rs1 << 15 | funct3 << 12 | 0x63

with tempfile.TemporaryDirectory(prefix="riscv-load-branches-") as directory:
    work = Path(directory)
    binary = work / "sim"
    subprocess.run([
        "cc", "-Wall", "-Wextra", "-Werror", "-g",
        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-o", str(binary),
        *[str(root / name) for name in
          ("main.c", "disassembler.c", "pipeline.c", "utilities.c")],
    ], check=True)
    for name, funct3 in (("beq", 0), ("bne", 1), ("blt", 4), ("bge", 5)):
        for operand in (1, 2):
            for value in (-1, 0, 1):
                words = [0x13] * 26 + [value & 0xffffffff] + [0] * 9
                words[0] = 0x25800093
                words[8] = 0x0000a283
                rs1, rs2 = (5, 0) if operand == 1 else (0, 5)
                words[9] = branch(rs1, rs2, funct3)
                words[15] = 0x06300313
                words[17] = 0x00700393
                words[21] = 0x8067
                a, b = (value, 0) if operand == 1 else (0, value)
                taken = {"beq": a == b, "bne": a != b,
                         "blt": a < b, "bge": a >= b}[name]
                source, output = work / "input.txt", work / "output.txt"
                source.write_text("".join(f"{word:032b}\n" for word in words))
                subprocess.run([str(binary), str(source), str(output), "sim"],
                               check=True, timeout=5)
                summary = output.read_text().split("**** Summary", 1)[1]
                registers = summary.split("Integer Registers:\n", 1)[1].split("Data Memory:", 1)[0]
                values = {int(r): int(v) for r, v in re.findall(r"R(\d+)\s+(-?\d+)", registers)}
                expected = 0 if taken else 99
                if values[5] != value or values[6] != expected or values[7] != 7:
                    raise SystemExit(f"FAIL: {name} load-rs{operand} value={value}: "
                                     f"R5={values[5]}, R6={values[6]}, R7={values[7]}")
                print(f"PASS: {name} load-rs{operand} value={value}")

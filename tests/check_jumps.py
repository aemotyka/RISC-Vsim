from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def jump(offset):
    return (((offset >> 20) & 1) << 31
            | ((offset >> 1) & 0x3ff) << 21
            | ((offset >> 11) & 1) << 20
            | ((offset >> 12) & 0xff) << 12
            | 0x6f)

with tempfile.TemporaryDirectory(prefix="riscv-jumps-") as directory:
    work = Path(directory)
    binary = work / "sim"
    subprocess.run([
        "cc", "-Wall", "-Wextra", "-Werror", "-g",
        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-o", str(binary),
        *[str(root / name) for name in
          ("main.c", "disassembler.c", "pipeline.c", "utilities.c")],
    ], check=True)

    for name in ("forward", "backward"):
        words = [0x13] * 26 + [0] * 10
        words[0] = jump(32)
        words[12] = 0x00008067
        if name == "forward":
            words[6] = 0x06300313
            words[8] = 0x00700293
        else:
            words[1] = 0x00700293
            words[2] = jump(40)
            words[8] = jump(-28)

        source = work / f"{name}.txt"
        output = work / f"{name}.out"
        source.write_text("".join(f"{word:032b}\n" for word in words))
        subprocess.run(
            [str(binary), str(source), str(output), "sim"],
            check=True, timeout=5,
        )
        trace = output.read_text().split("**** Summary", 1)[0]
        last_cycle = trace.rsplit("***** Cycle #", 1)[1]
        registers = last_cycle.split("Integer Registers:\n", 1)[1]
        registers = registers.split("\nData Memory:", 1)[0]
        values = {
            int(register): int(value)
            for register, value in re.findall(r"R(\d+)\s+(-?\d+)", registers)
        }
        if values[5] != 7 or values[6] != 0:
            raise SystemExit(f"FAIL: {name}: R5={values[5]}, R6={values[6]}")
        print(f"PASS: {name} jump")

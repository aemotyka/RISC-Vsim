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

    for name in ("forward", "backward", "jal", "jalr", "jalr-x0", "x0"):
        words = [0x13] * 26 + [0] * 10
        words[0] = jump(32)
        words[12] = 0x00008067
        if name == "forward":
            words[6] = 0x06300313
            words[8] = 0x00700293
        elif name == "backward":
            words[1] = 0x00700293
            words[2] = jump(40)
            words[8] = jump(-28)
        elif name in ("jal", "jalr", "jalr-x0"):
            words[6] = 0x06300313
            words[8] = 0x00700293
            if name == "jal":
                words[0] = jump(32) | 0x80
            else:
                words[0] = 0x21500113
                words[1] = 0xffb100e7 if name == "jalr" else 0xffb10067
        else:
            words[0] = 0x00900013
            words[1] = 0x00700293

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
        if values[0] != 0 or values[5] != 7 or values[6] != 0:
            raise SystemExit(f"FAIL: {name}: R0={values[0]}, R5={values[5]}, R6={values[6]}")
        if name in ("jal", "jalr"):
            expected_link = 500 if name == "jal" else 504
            if values[1] != expected_link:
                raise SystemExit(f"FAIL: {name}: R1={values[1]}, expected {expected_link}")
        print(f"PASS: {name}")

    for offset in (-1048576, -65536, -28, 32, 65536, 524288, 1048574):
        for rd in (0, 1):
            source = work / "dis-input.txt"
            output = work / "dis-output.txt"
            source.write_text(f"{jump(offset) | rd << 7:032b}\n{0x8067:032b}\n")
            subprocess.run([str(binary), str(source), str(output), "dis"],
                           check=True, timeout=5)
            expected = f"J\t\t//JAL x0, {offset}" if rd == 0 else f"JAL x1, {offset}"
            if expected not in output.read_text():
                raise SystemExit(f"FAIL: disassembly: {expected}")
        print(f"PASS: jump disassembly {offset}")

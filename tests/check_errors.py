from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
flags = ["-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined",
         "-fno-sanitize-recover=all"]

with tempfile.TemporaryDirectory(prefix="riscv-errors-") as directory:
    work = Path(directory)
    binary = work / "sim"
    subprocess.run(["cc", *flags, "-o", str(binary),
                    *[str(root / name) for name in
                      ("main.c", "disassembler.c", "pipeline.c", "utilities.c")]], check=True)
    valid = (root / "tests/fixtures/fibonacci.txt").read_text()
    cases = [
        ("empty", "", "sim", [], "32 binary digits"),
        ("short", "0" * 31 + "\n", "dis", [], "32 binary digits"),
        ("long", "0" * 33 + "\n", "dis", [], "32 binary digits"),
        ("invalid-digit", "0" * 31 + "2\n", "dis", [], "32 binary digits"),
        ("missing-memory", f"{0x8067:032b}\n", "sim", [], "is absent"),
        ("jump-beyond-input", f"{0x4000006f:032b}\n" + ("0" * 32 + "\n") * 35,
         "sim", [], "is absent"),
        ("bad-operation", valid, "unknown", [], "Unsupported operation"),
        ("negative-trace", valid, "sim", ["T-1:4"], "Invalid trace format"),
        ("reversed-trace", valid, "sim", ["T4:1"], "Invalid trace format"),
        ("trailing-trace", valid, "sim", ["T0:4junk"], "Invalid trace format"),
    ]
    for name, content, mode, extra, message in cases:
        source, output = work / "input.txt", work / "output.txt"
        source.write_text(content)
        result = subprocess.run([str(binary), str(source), str(output), mode, *extra],
                                capture_output=True, text=True, timeout=5)
        if result.returncode != 1 or message not in result.stderr:
            raise SystemExit(f"FAIL: {name}: {result.returncode}: {result.stderr}")
        print(f"PASS: rejected {name}")

    source = work / "input.txt"
    source.write_text(valid)
    for name, input_path, output_path, message in (
        ("missing-input", work / "absent", work / "out", "Could not open input"),
        ("output-open", source, work, "Could not open output"),
    ):
        result = subprocess.run([str(binary), str(input_path), str(output_path), "dis"],
                                capture_output=True, text=True, timeout=5)
        if result.returncode != 1 or message not in result.stderr:
            raise SystemExit(f"FAIL: {name}: {result.stderr}")
        print(f"PASS: rejected {name}")

    reference = None
    for name, content in (("LF", valid), ("CRLF", valid.replace("\n", "\r\n")),
                          ("no-final-newline", valid.rstrip("\n"))):
        source.write_bytes(content.encode())
        output = work / "accepted.out"
        subprocess.run([str(binary), str(source), str(output), "sim"], check=True, timeout=5)
        if reference is not None and output.read_bytes() != reference:
            raise SystemExit(f"FAIL: {name} changed the trace")
        reference = output.read_bytes()
        print(f"PASS: accepted {name}")

    harness = work / "allocation.c"
    harness.write_text(r'''#include <stdlib.h>
#include <string.h>
#include "utilities.h"
void *test_malloc(size_t bytes) { (void)bytes; return NULL; }
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    FILE *output = fopen(argv[2], "w");
    if (!output) return 2;
    fputs("flushed\n", output);
    int size;
    const char *slices[] = {"0", "1"};
    if (strcmp(argv[1], "slice") == 0) processSlice("01", 0, 1, &size);
    else if (strcmp(argv[1], "combine") == 0) combineSlices(slices, 2);
    else if (strcmp(argv[1], "shift") == 0) shiftLeft("01");
    else if (strcmp(argv[1], "input") == 0) {
        FILE *input = tmpfile();
        if (!input) return 2;
        fputs("00000000000000000000000000000000\n", input);
        rewind(input);
        load_input(input);
    }
    else readSpecificLine(output, 497);
    return 2;
}
''')
    obj = work / "utilities.o"
    subprocess.run(["cc", *flags, "-Dmalloc=test_malloc", "-c", str(root / "utilities.c"),
                    "-o", str(obj)], check=True)
    fault = work / "fault"
    subprocess.run(["cc", *flags, "-I", str(root), str(harness), str(obj), "-o", str(fault)],
                   check=True)
    for name in ("slice", "combine", "shift", "input", "unaligned-address"):
        output = work / "flush.out"
        result = subprocess.run([str(fault), name, str(output)],
                                capture_output=True, text=True, timeout=5)
        message = "Invalid input address" if name == "unaligned-address" else "Memory allocation failed"
        if result.returncode != 1 or message not in result.stderr or output.read_text() != "flushed\n":
            raise SystemExit(f"FAIL: {name}: {result.stderr}")
        print(f"PASS: {name} exits cleanly and flushes open streams")

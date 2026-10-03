# RISC-V pipeline simulator

Eight-stage pipeline simulator in C.

## Build and run

```sh
cc -std=c11 -Wall -Wextra -Werror -o RISC-Vsim main.c disassembler.c pipeline.c utilities.c
./RISC-Vsim input.txt output.txt dis
./RISC-Vsim input.txt output.txt sim T0:250
```

The trace range is optional. Without it, simulation prints cycles 0–250 and the final summary.

## Tests

Each script builds a temporary binary with AddressSanitizer and UndefinedBehaviorSanitizer.

```sh
python3 tests/check_fibonacci.py
python3 tests/check_jumps.py
python3 tests/check_branches.py
python3 tests/check_load_branches.py
python3 tests/check_immediates.py
python3 tests/check_arithmetic.py
python3 tests/check_errors.py
```

[Instructions and memory layout](docs/model.md) · [Performance measurements](docs/performance.md)

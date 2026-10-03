Compile before running.

To compile:
gcc -o RISC-Vsim main.c disassembler.c pipeline.c utilities.c

To dissassemble:
./RISC-Vsim inputfilename outputfilename dis

To simulate pipeline:
./RISC-Vsim inputfilename outputfilename sim <T{trace start}:{trace end}>

Model, memory layout, supported instructions, and validation: [docs/model.md](docs/model.md).

Performance results: [docs/performance.md](docs/performance.md).

Compile before running.

To compile:
gcc -o RISC-Vsim main.c disassembler.c

To dissassemble:
./RISC-Vsim inputfilename outputfilename dis

To simulate pipeline:
./RISC-Vsim inputfilename outputfilename sim <T{trace start}:{trace end}>

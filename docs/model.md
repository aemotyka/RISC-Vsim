# Pipeline model

Supported instructions: ADD, SUB, AND, OR, XOR, SLL, SRL, SLT, ADDI, SLTI, LW, SW, BEQ, BNE, BLT, BGE, JAL, JALR. J is JAL to x0; NOP is ADDI x0, x0, 0.

Registers and arithmetic are 32 bits. Addition/subtraction wrap. SLT, SLTI, BLT, and BGE use signed comparisons. SRL is logical; shifts use the low five count bits. x0 stays zero.

Stages: IF, IS, ID, RF, EX, DF, DS, WB.

## Input and memory

One 32-bit binary word per line. LF, CRLF, and a missing final newline are accepted.

Word zero is at address 496. Records 26–35 initialize ten data words at addresses 600–636 and must be present. Code extending past this region must jump over it.

LW/SW use a separate data array; stores do not change instruction input. Only aligned word addresses 600–636 are supported. Invalid data accesses are not consistently rejected.

`0x00008067` is RET: it halts instead of returning to x1. Keep NOP padding after it for pipeline drain. The disassembler prints words after RET as data.

## Trace

`Tstart:end` selects inclusive trace cycles. The default is 0–250. The final summary is always written.

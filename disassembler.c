#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Function to convert an integer to a binary string
void to_binary_string(uint32_t value, char *buffer, int bits) {
    buffer[bits] = '\0'; // Null-terminate the string
    for (int i = bits - 1; i >= 0; --i) {
        buffer[i] = (value & 1) ? '1' : '0'; // Set '1' or '0' based on the least significant bit
        value >>= 1; // Shift right
    }
}

// Main code to process input and disassemble it
void disassemble(FILE* input_file, FILE* output_file) {
    char line[1024];
    int pc = 0;
    int start_address = 496;
    bool returned = false;

    // Loop through each line in the input file
    while (fgets(line, sizeof(line), input_file)) {

        // Remove the newline character from the end of the line, if present
        line[strcspn(line, "\n")] = '\0';

        // Convert the line (binary string) to an unsigned integer
        uint32_t instruction = (uint32_t)strtoul(line, NULL, 2);

        // Perform the bit shifting to extract parts
        uint32_t funct7 = (instruction >> 26) & 0x3F;
        uint32_t rs2 = (instruction >> 20) & 0x3F;
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x07;
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t opcode = instruction & 0x7F;

        // Buffer to hold the binary strings + null terminator
        char binary_funct7[7];
        char binary_rs2[7];
        char binary_rs1[6];
        char binary_funct3[4];
        char binary_rd[6];
        char binary_opcode[8];

        // Convert each part to binary string
        to_binary_string(funct7, binary_funct7, 6);
        to_binary_string(rs2, binary_rs2, 6);
        to_binary_string(rs1, binary_rs1, 5);
        to_binary_string(funct3, binary_funct3, 3);
        to_binary_string(rd, binary_rd, 5);
        to_binary_string(opcode, binary_opcode, 7);

        // Print decimal integer representation of data if RET has been called
        if (returned) {
            fprintf(output_file, "%s%s%s%s%s%s\t%i %i", binary_funct7, binary_rs2, binary_rs1, binary_funct3, binary_rd, binary_opcode, (start_address + 4 * pc), instruction);
        }
        // Otherwise, check opcode, then determine what assembly function it is
        else {
            // Print each binary part to the output file, along with the address
            fprintf(output_file, "%s %s %s %s %s %s\t%i\t", binary_funct7, binary_rs2, binary_rs1, binary_funct3, binary_rd, binary_opcode, (start_address + 4 * pc));
            // Opcode R
            if (strcmp(binary_opcode, "0110011") == 0) {
                // ADD
                if (strcmp(binary_funct3, "000") == 0 && strcmp(binary_funct7, "000000") == 0) {

                    fprintf(output_file, "ADD x%u, x%u, x%u", rd, rs1, rs2);
                }
                // SUB
                else if (strcmp(binary_funct3, "000") == 0 && strcmp(binary_funct7, "010000") == 0)
                {
                    fprintf(output_file, "SUB x%u, x%u, x%u", rd, rs1, rs2);
                }
                // SLL
                else if (strcmp(binary_funct3, "001") == 0 && strcmp(binary_funct7, "010000") == 0)
                {
                    fprintf(output_file, "SLL x%u, x%u, x%u", rd, rs1, rs2);
                }
                // SLT
                else if (strcmp(binary_funct3, "010") == 0 && strcmp(binary_funct7, "010000") == 0)
                {
                    fprintf(output_file, "SLT x%u, x%u, x%u", rd, rs1, rs2);
                }
                // XOR
                else if (strcmp(binary_funct3, "100") == 0 && strcmp(binary_funct7, "010000") == 0)
                {
                    fprintf(output_file, "XOR x%u, x%u, x%u", rd, rs1, rs2);
                }
                // SRL
                else if (strcmp(binary_funct3, "101") == 0 && strcmp(binary_funct7, "010000") == 0)
                {
                    fprintf(output_file, "SRL x%u, x%u, x%u", rd, rs1, rs2);
                }
                // OR
                else if (strcmp(binary_funct3, "110") == 0 && strcmp(binary_funct7, "010000") == 0)
                {
                    fprintf(output_file, "OR x%u, x%u, x%u", rd, rs1, rs2);
                }
                // AND
                else if (strcmp(binary_funct3, "111") == 0 && strcmp(binary_funct7, "010000") == 0)
                {
                    fprintf(output_file, "AND x%u, x%u, x%u", rd, rs1, rs2);
                }
            }
            // Opcode S
            else if (strcmp(binary_opcode, "0100011") == 0)
            {
                // SW
                if (strcmp(binary_funct3, "010") == 0)
                {
                    int32_t immediate_ = (funct7 << 6) | rd; // Calculate immediate value
                    fprintf(output_file, "SW x%u %u(x%u)", rs2, immediate_, rs1);
                }
            }
            // Opcode B
            else if (strcmp(binary_opcode, "1100011") == 0)
            {
                // Calculate immediate value for all Opcode B operations
                uint32_t imm12 = (instruction >> 31) & 0x1;
                uint32_t imm10_5 = (instruction >> 25) & 0x3F;
                uint32_t imm4_1 = (instruction >> 8) & 0xF;
                uint32_t imm11 = (instruction >> 7) & 0x1;

                int32_t immediate_ = (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);

                // Perform sign extension if imm12 is set
                if (imm12) {
                    immediate_ |= 0xFFFFE000;
                }

                // BEQ
                if (strcmp(binary_funct3, "000") == 0)
                {
                    fprintf(output_file, "BEQ x%u, x%u, %d", rs1, rs2, immediate_);
                }
                // BNE
                else if (strcmp(binary_funct3, "001") == 0)
                {
                    fprintf(output_file, "BNE x%u, x%u, %d", rs1, rs2, immediate_);
                }
                // BLT
                else if (strcmp(binary_funct3, "100") == 0)
                {
                    fprintf(output_file, "BLT x%u, x%u, %d", rs1, rs2, immediate_);
                }
                // BGE
                else if (strcmp(binary_funct3, "101") == 0)
                {
                    fprintf(output_file, "BGE x%u, x%u, %d", rs1, rs2, immediate_);
                }
            }
            // Opcode I
            else if (strcmp(binary_opcode, "1100111") == 0)
            {
                // JALR and RET
                if (strcmp(binary_funct3, "000") == 0)
                {
                    int32_t immediate_ = (funct7 << 6) | rs2;
                    // RET
                    if (strcmp(binary_rd, "00000") == 0 && strcmp(binary_rs1, "00001") == 0 && !immediate_)
                    {
                        returned = true;
                        fprintf(output_file, "RET\t\t//JALR x0, x1, 0");
                    }
                    // JALR
                    else {
                        fprintf(output_file, "JALR x%u, x%u, x%d", rd, rs1, immediate_);
                    }
                }
            }
            // Still Opcode I
            else if (strcmp(binary_opcode, "0000011") == 0)
            {
                // LW
                if (strcmp(binary_funct3, "010") == 0)
                {
                    int32_t immediate_ = (funct7 << 6) | rs2; // Calculate immediate value
                    fprintf(output_file, "LW x%u, %d(x%u)", rd, immediate_, rs1);
                }
            }              
            // Still Opcode I
            else if (strcmp(binary_opcode, "0010011") == 0)
            {
                int32_t immediate_ = (funct7 << 6) | rs2; // Calculate immediate value

                if (funct7 & 0x20) {  // Check if the 6th bit of funct7 is set (0x20 = 0010 0000)
                    immediate_ |= 0xFFFFF000; // Set the upper bits for sign extension (assuming a 12-bit immediate)
                }
                
                // ADDI and NOP
                if (strcmp(binary_funct3, "000") == 0)
                {
                    // NOP
                    if (strcmp(binary_rd, "00000") == 0 && strcmp(binary_rs1, "00000") == 0 && !immediate_) {
                        fprintf(output_file, "NOP\t\t//ADDI x0, x0, 0");
                    }
                    // ADDI
                    else {
                        fprintf(output_file, "ADDI x%u, x%u, %d", rd, rs1, immediate_);
                    }
                }
                // SLTI
                else if (strcmp(binary_funct3, "010") == 0)
                {
                    fprintf(output_file, "SLTI x%u, x%u, %d", rd, rs1, immediate_);
                }
            }
            // Opcode J
            else if (strcmp(binary_opcode, "1101111") == 0)
            {
                // JAL
                if (true) {
                    // Calculate immediate value
                    uint32_t imm20 = (instruction >> 31) & 0x1;
                    uint32_t imm19_12 = (instruction >> 12) & 0xFF;
                    uint32_t imm11 = (instruction >> 20) & 0x1;
                    uint32_t imm10_1 = (instruction >> 21) & 0x3FF;

                    int32_t immediate_ = (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);

                    // Perform sign extension if imm20 is set
                    if (imm20) {
                        immediate_ |= 0xFFF00000;
                    }
                    if (strcmp(binary_rd, "00000") == 0) {
                        fprintf(output_file, "J\t\t//JAL x0, %d", immediate_);
                    }
                    else {
                        fprintf(output_file, "JAL x%u, %d", rd, immediate_);
                    }
                }
            }
        }

        // Move to next line in output
        fprintf(output_file, "\n");

        // Increment current process pointer
        pc++;
    }
}
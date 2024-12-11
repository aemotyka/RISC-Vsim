#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "utilities.h"

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

        // Use for immediate value later
        char instruction_[32];
        to_binary_string(instruction, instruction_, 32);
        int size;

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
                else if (strcmp(binary_funct3, "001") == 0 && strcmp(binary_funct7, "000000") == 0)
                {
                    fprintf(output_file, "SLL x%u, x%u, x%u", rd, rs1, rs2);
                }
                // SLT
                else if (strcmp(binary_funct3, "010") == 0 && strcmp(binary_funct7, "000000") == 0)
                {
                    fprintf(output_file, "SLT x%u, x%u, x%u", rd, rs1, rs2);
                }
                // XOR
                else if (strcmp(binary_funct3, "100") == 0 && strcmp(binary_funct7, "000000") == 0)
                {
                    fprintf(output_file, "XOR x%u, x%u, x%u", rd, rs1, rs2);
                }
                // SRL
                else if (strcmp(binary_funct3, "101") == 0 && strcmp(binary_funct7, "000000") == 0)
                {
                    fprintf(output_file, "SRL x%u, x%u, x%u", rd, rs1, rs2);
                }
                // OR
                else if (strcmp(binary_funct3, "110") == 0 && strcmp(binary_funct7, "000000") == 0)
                {
                    fprintf(output_file, "OR x%u, x%u, x%u", rd, rs1, rs2);
                }
                // AND
                else if (strcmp(binary_funct3, "111") == 0 && strcmp(binary_funct7, "000000") == 0)
                {
                    fprintf(output_file, "AND x%u, x%u, x%u", rd, rs1, rs2);
                }
            }
            // Opcode S
            else if (strcmp(binary_opcode, "0100011") == 0)
            {
                // Calculate immediate value:
                char *bits_11_5 = processSlice(instruction_, 0, 7, &size);
                char *bits_4_0 = processSlice(instruction_, 20, 25, &size);

                const char *opcode_i_slices[] = {bits_11_5, bits_4_0};
                char *immediate_ = combineSlices(opcode_i_slices, 2);
                int immediate = (int)strtol(immediate_, NULL, 2);

                // Sign extend immediate
                if (immediate & 0x800) {
                        immediate |= 0xF800;
                    }

                // SW
                if (strcmp(binary_funct3, "010") == 0)
                {
                    fprintf(output_file, "SW x%u %hi(x%u)", rs2, immediate, rs1);
                }

                // Free dynamically allocated memory
                free(bits_11_5);
                free(bits_4_0);
                free(immediate_);
            }
            // Opcode B
            else if (strcmp(binary_opcode, "1100011") == 0)
            {
                // Calculate immediate value:
                char *bits_12 = processSlice(instruction_, 0, 1, &size);
                char *bits_10_5 = processSlice(instruction_, 1, 7, &size);
                char *bits_4_1 = processSlice(instruction_, 20, 24, &size);
                char *bits_11 = processSlice(instruction_, 24, 25, &size);

                const char *opcode_i_slices[] = {bits_12, bits_11, bits_10_5, bits_4_1};
                char *immediate_ = combineSlices(opcode_i_slices, 4);
                char *shifted = shiftLeft(immediate_);
                int immediate = (int)strtol(shifted, NULL, 2);

                // Perform sign extension
                if (immediate & 0x800) {
                    immediate |= 0xFFFFE000;
                }

                // BEQ
                if (strcmp(binary_funct3, "000") == 0)
                {
                    fprintf(output_file, "BEQ x%u, x%u, %d", rs1, rs2, immediate);
                }
                // BNE
                else if (strcmp(binary_funct3, "001") == 0)
                {
                    fprintf(output_file, "BNE x%u, x%u, %d", rs1, rs2, immediate);
                }
                // BLT
                else if (strcmp(binary_funct3, "100") == 0)
                {
                    fprintf(output_file, "BLT x%u, x%u, %d", rs1, rs2, immediate);
                }
                // BGE
                else if (strcmp(binary_funct3, "101") == 0)
                {
                    fprintf(output_file, "BGE x%u, x%u, %d", rs1, rs2, immediate);
                }

                // Free dynamically allocated memory
                free(bits_12);
                free(bits_10_5);
                free(bits_4_1);
                free(bits_11);
                free(immediate_);
                free(shifted);
            }
            // Opcode I
            else if (strcmp(binary_opcode, "1100111") == 0)
            {
                // Calculate immediate value:
                char *bits_0_11 = processSlice(instruction_, 0, 12, &size);
                const char *opcode_i_slices[] = {bits_0_11};
                char *immediate_ = combineSlices(opcode_i_slices, 1);
                int immediate = (int)strtol(immediate_, NULL, 2);

                // Sign extend immediate
                if (immediate & 0x800) {
                        immediate |= 0xF000;
                    }

                // JALR and RET
                if (strcmp(binary_funct3, "000") == 0)
                {
                    // RET
                    if (strcmp(binary_rd, "00000") == 0 && strcmp(binary_rs1, "00001") == 0 && !immediate)
                    {
                        returned = true;
                        fprintf(output_file, "RET\t\t//JALR x0, x1, 0");
                    }
                    // JALR
                    else {
                        fprintf(output_file, "JALR x%u, x%u, %hi", rd, rs1, immediate);
                    }
                }

                // Free dynamically allocated memory
                free(bits_0_11);
                free(immediate_);
            }
            // Still Opcode I
            else if (strcmp(binary_opcode, "0000011") == 0)
            {
                // Calculate immediate value:
                char *bits_0_11 = processSlice(instruction_, 0, 12, &size);
                const char *opcode_i_slices[] = {bits_0_11};
                char *immediate_ = combineSlices(opcode_i_slices, 1);
                int immediate = (int)strtol(immediate_, NULL, 2);

                // Sign extend immediate
                if (immediate & 0x800) {
                        immediate |= 0xF000;
                    }
                
                // LW
                if (strcmp(binary_funct3, "010") == 0)
                {
                    fprintf(output_file, "LW x%u, %hi(x%u)", rd, immediate, rs1);
                }

                // Free dynamically allocated memory
                free(bits_0_11);
                free(immediate_);
            }              
            // Still Opcode I
            else if (strcmp(binary_opcode, "0010011") == 0)
            {
                // Calculate immediate value:
                char *bits_0_11 = processSlice(instruction_, 0, 12, &size);
                const char *opcode_i_slices[] = {bits_0_11};
                char *immediate_ = combineSlices(opcode_i_slices, 1);
                int immediate = (int)strtol(immediate_, NULL, 2);

                // Sign extend immediate
                if (immediate & 0x800) {
                        immediate |= 0xF000;
                    }
                
                // ADDI and NOP
                if (strcmp(binary_funct3, "000") == 0)
                {
                    // NOP
                    if (strcmp(binary_rd, "00000") == 0 && strcmp(binary_rs1, "00000") == 0 && !immediate) {
                        fprintf(output_file, "NOP\t\t//ADDI x0, x0, 0");
                    }
                    // ADDI
                    else {
                        fprintf(output_file, "ADDI x%u, x%u, %hi", rd, rs1, immediate);
                    }
                }
                // SLTI
                else if (strcmp(binary_funct3, "010") == 0)
                {
                    fprintf(output_file, "SLTI x%u, x%u, %hi", rd, rs1, immediate);
                }

                // Free dynamically allocated memory
                free(bits_0_11);
                free(immediate_);
            }
            // Opcode J
            else if (strcmp(binary_opcode, "1101111") == 0)
            {
                // Calculate immediate value:
                char *bits_20 = processSlice(instruction_, 0, 1, &size);
                char *bits_12_19 = processSlice(instruction_, 12, 20, &size);
                char *bits_11 = processSlice(instruction_, 11, 12, &size);
                char *bits_1_10 = processSlice(instruction_, 1, 11, &size);

                const char *opcode_i_slices[] = {bits_20, bits_12_19, bits_11, bits_1_10};
                char *immediate_ = combineSlices(opcode_i_slices, 4);
                char *shifted = shiftLeft(immediate_);
                int immediate = (int)strtol(shifted, NULL, 2);

                // Sign extend the immediate value
                if (immediate & 0x80000) {
                    immediate |= 0xFF00000;  // Sign-extend to 32-bits
                }
                
                if (true) {
                    // J
                    if (strcmp(binary_rd, "00000") == 0) {
                        fprintf(output_file, "J\t\t//JAL x0, %hi", immediate);
                    }
                    // JAL
                    else {
                        fprintf(output_file, "JAL x%u, %hi", rd, immediate);
                    }
                }

                // Free dynamically allocated memory
                free(bits_20);
                free(bits_12_19);
                free(bits_11);
                free(bits_1_10);
                free(immediate_);
                free(shifted);
            }
        }

        // Move to next line in output
        fprintf(output_file, "\n");

        // Increment current process pointer
        pc++;
    }
}
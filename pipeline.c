#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "utilities.h"

int register_file[32];
int data_memory[10];

typedef struct {
    bool returned;
    bool branch_next_instruction;
    int forwards;
    char ex_df_to_rf_ex[50];
    char df_ds_to_ex_df[50];
    char df_ds_to_rf_ex[50];
    char ds_wb_to_ex_df[50];
    char ds_wb_to_rf_ex[50];
    char ex_df_to_rf_ex_out[50];
    char df_ds_to_ex_df_out[50];
    char df_ds_to_rf_ex_out[50];
    char ds_wb_to_ex_df_out[50];
    char ds_wb_to_rf_ex_out[50];
    int _ex_df_to_rf_ex;
    int _df_ds_to_ex_df;
    int _df_ds_to_rf_ex;
    int _ds_wb_to_ex_df;
    int _ds_wb_to_rf_ex;
    int break_pc;
    u_int32_t cycle;
    int branch_stalls;
    int load_stalls;
    int other_stalls;
    int load_stall_length;
    bool load_stall;
    bool load_stall_new;
    char stall_inst[50];
    char forwarding_inst[100];
} global_state;

typedef struct {
    uint32_t NPC;
    bool branch_taken;
    uint32_t branch_target;
    FILE* input_file;
    char status[50];
    char instruction[10];
    bool init;
    int num_stalls;
    int branch_counter;
} IF_IS;

typedef struct {
    char _instruction[10];
    uint32_t instruction;
    char status[50];
} IS_ID;

typedef struct {
    char instruction[10];
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    int imm;
    bool hazard_detected;
    bool returned;
    char status[50];
    bool no_op_no;
} ID_RF;

typedef struct {
    char instruction[10];
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    u_int16_t rs1_data;
    u_int16_t rs2_data;
    int imm;
    char status[50];
    bool stalled;
    bool rs1_forwarded;
    bool rs2_forwarded;
} RF_EX;

typedef struct {
    char instruction[10];
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    u_int16_t rs1_data;
    u_int16_t rs2_data;
    int imm;
    u_int16_t alu_result;
    int branch_target;
    bool branch_taken;
    bool mem_read;
    bool mem_write;
    char status[50];
    bool rs1_forwarded;
    bool rs2_forwarded;
} EX_DF;

typedef struct {
    char instruction[10];
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    int imm;
    uint32_t mem_address;
    bool mem_read;
    bool mem_write;
    uint32_t mem_data;
    char status[50];
    u_int16_t alu_result;
    bool rs1_forwarded;
    bool rs2_forwarded;
} DF_DS;

typedef struct {
    char instruction[10];
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    int imm;
    uint32_t mem_data;
    char status[50];
    uint32_t alu_result;
    bool rs1_forwarded;
    bool rs2_forwarded;
} DS_WB;

// Not sure if i need this one
typedef struct {
    char instruction[10];
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    int imm;
    uint32_t write_data;
    char status[50];
} WB_out;

IF_IS if_is;
IS_ID is_id;
ID_RF id_rf;
RF_EX rf_ex;
EX_DF ex_df;
DF_DS df_ds;
DS_WB ds_wb;
WB_out wb;

global_state CPUstate;

typedef struct {
    int data;
    uint8_t reg;
} ForwardingPath;

ForwardingPath ex_df_to_rf_ex;
ForwardingPath df_ds_to_ex_df;
ForwardingPath df_ds_to_rf_ex;
ForwardingPath ds_wb_to_ex_df;
ForwardingPath ds_wb_to_rf_ex;

void IF() {
    if (if_is.init) {
        if_is.init = false;
        if_is.NPC += 4;
        return;
    }

    if (ex_df.branch_taken) {
        sprintf(if_is.instruction, "**STALL**");
        ex_df.branch_taken = false;
        if_is.branch_counter = 3;
        CPUstate.branch_stalls++;

        return;
    }

    if (if_is.branch_counter > 0) {
        CPUstate.branch_stalls++;
        if_is.branch_counter--;
    }

    if ((!ex_df.branch_taken) && (ex_df.branch_target != 0)) {        
        if_is.NPC = ex_df.branch_target + 4;
        ex_df.branch_target = 0;
        return;
    }

    if (CPUstate.load_stall_length > 1) {
        CPUstate.load_stall_length--;
        if (CPUstate.load_stall_length == 2) {
            CPUstate.load_stalls++;
        }
        return;
    } else if (CPUstate.load_stall_length == 1) {
        CPUstate.load_stall_length--;
        CPUstate.load_stalls++;
    }

    if (if_is.branch_taken) {
        if_is.NPC = if_is.branch_target;
        if_is.branch_taken = false;
    } else {
        if_is.NPC += 4;
    }
};

void IS() {
    if (CPUstate.load_stall_length > 1) {
        return;
    }

    if (strcmp(if_is.instruction, "**STALL**") == 0) {
        sprintf(is_id.status, "**STALL**");
        sprintf(is_id._instruction, "**STALL**");
    }
    
    if (ex_df.branch_taken) {
        sprintf(is_id.status, "**STALL**");
        sprintf(is_id._instruction, "**STALL**");
        is_id.instruction = 0;
        return;
    }

    if (if_is.NPC == 496) {
        strcpy(is_id._instruction, "NOP");
        strcpy(is_id.status, "NOP");
        return;
    }


    is_id.instruction = readSpecificLine(if_is.input_file, if_is.NPC-4);

    u_int8_t rs2 = (is_id.instruction >> 20) & 0x3F;
    u_int8_t rs1 = (is_id.instruction >> 15) & 0x1F;
    u_int8_t rd = (is_id.instruction >> 7) & 0x1F;
    uint32_t opcode = is_id.instruction & 0x7F;

    if (id_rf.returned) {
        strcpy(is_id._instruction, "NOP");
        strcpy(is_id.status, "NOP");
    } else {
        strcpy(is_id._instruction, "");
        sprintf(is_id.status, "%X %X %X %X", opcode , rd, rs1, rs2);
    }
};

void ID() {
    id_rf.no_op_no = false;

    if (strcmp(is_id.status, "**STALL**") == 0) {
    }

    if (ex_df.branch_taken) {
        sprintf(id_rf.status, "**STALL**");
        sprintf(id_rf.instruction, "**STALL**");
        id_rf.rd = 0;
        id_rf.rs1 = 0;
        id_rf.rs2 = 0;
        id_rf.imm = 0;
        return;
    }

    if (CPUstate.load_stall_length > 0) {
        return;
    }

    id_rf.rs2 = (is_id.instruction >> 20) & 0x3F;
    id_rf.rs1 = (is_id.instruction >> 15) & 0x1F;
    id_rf.rd = (is_id.instruction >> 7) & 0x1F;

    uint32_t funct7 = (is_id.instruction >> 26) & 0x3F;
    uint32_t funct3 = (is_id.instruction >> 12) & 0x07;
    uint32_t opcode = is_id.instruction & 0x7F;

    char binary_funct7[7];
    char binary_rs2[7];
    char binary_rs1[6];
    char binary_funct3[4];
    char binary_rd[6];
    char binary_opcode[8];

    to_binary_string(funct7, binary_funct7, 6);
    to_binary_string(id_rf.rs2, binary_rs2, 6);
    to_binary_string(id_rf.rs1, binary_rs1, 5);
    to_binary_string(funct3, binary_funct3, 3);
    to_binary_string(id_rf.rd, binary_rd, 5);
    to_binary_string(opcode, binary_opcode, 7);

    if (id_rf.returned) {
        strcpy(id_rf.instruction, "NOP");
        strcpy(id_rf.status, "NOP");
        id_rf.imm = 0;
        id_rf.rd = 0;
        id_rf.rs1 = 0;
        id_rf.rs2 = 0;
    }

    if (strcmp(is_id._instruction, "NOP") == 0) {
        strcpy(is_id._instruction, "");
        strcpy(id_rf.instruction, "NOP");
        id_rf.imm = 0;
        id_rf.rd = 0;
        id_rf.rs1 = 0;
        id_rf.rs2 = 0;
    } else if (strcmp(is_id._instruction, "**STALL**") == 0) {
        
        strcpy(id_rf.instruction, "**STALL**");
        strcpy(id_rf.status, "**STALL**");
        id_rf.no_op_no = true;
        id_rf.imm = 0;
        id_rf.rd = 0;
        id_rf.rs1 = 0;
        id_rf.rs2 = 0;
        return;
    } else {
        // Opcode R
        if (strcmp(binary_opcode, "0110011") == 0) {
                // ADD
                if (strcmp(binary_funct3, "000") == 0 && strcmp(binary_funct7, "000000") == 0) {
                    strcpy(id_rf.instruction, "ADD");
                    sprintf(id_rf.status, "%s R%u, R%u, R%u", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.rs2);
                    id_rf.imm = 0;
                }
                // SUB
                else if (strcmp(binary_funct3, "000") == 0 && strcmp(binary_funct7, "010000") == 0) {
                    strcpy(id_rf.instruction, "SUB");
                    sprintf(id_rf.status, "%s R%u, R%u, R%u", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.rs2);
                    id_rf.imm = 0;
                }
                // SLL
                else if (strcmp(binary_funct3, "001") == 0 && strcmp(binary_funct7, "000000") == 0) {
                    strcpy(id_rf.instruction, "SLL");
                    sprintf(id_rf.status, "%s R%u, R%u, R%u", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.rs2);
                    id_rf.imm = 0;
                }
                // SLT
                else if (strcmp(binary_funct3, "010") == 0 && strcmp(binary_funct7, "000000") == 0) {
                    strcpy(id_rf.instruction, "SLT");
                    sprintf(id_rf.status, "%s R%u, R%u, R%u", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.rs2);
                    id_rf.imm = 0;
                }
                // XOR
                else if (strcmp(binary_funct3, "100") == 0 && strcmp(binary_funct7, "000000") == 0) {
                    strcpy(id_rf.instruction, "XOR");
                    sprintf(id_rf.status, "%s R%u, R%u, R%u", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.rs2);
                    id_rf.imm = 0;
                }
                // SRL
                else if (strcmp(binary_funct3, "101") == 0 && strcmp(binary_funct7, "000000") == 0) {
                    strcpy(id_rf.instruction, "SRL");
                    sprintf(id_rf.status, "%s R%u, R%u, R%u", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.rs2);
                    id_rf.imm = 0;
                }
                // OR
                else if (strcmp(binary_funct3, "110") == 0 && strcmp(binary_funct7, "000000") == 0) {
                    strcpy(id_rf.instruction, "OR");
                    sprintf(id_rf.status, "%s R%u, R%u, R%u", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.rs2);
                    id_rf.imm = 0;
                }
                // AND
                else if (strcmp(binary_funct3, "111") == 0 && strcmp(binary_funct7, "000000") == 0) {
                    strcpy(id_rf.instruction, "AND");
                    sprintf(id_rf.status, "%s R%u, R%u, R%u", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.rs2);
                    id_rf.imm = 0;
                }
        }
        // Opcode S
        else if (strcmp(binary_opcode, "0100011") == 0) {
            // Calculate immediate value:
            char instruction_[32];
            to_binary_string(is_id.instruction, instruction_, 32);
            int size;

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
            if (strcmp(binary_funct3, "010") == 0) {
                strcpy(id_rf.instruction, "SW");
                id_rf.imm = immediate;
                sprintf(id_rf.status, "%s R%u, %hi(R%u)", id_rf.instruction, id_rf.rs2, id_rf.imm, id_rf.rs1);
                id_rf.rd = 0;
            }

            // Free dynamically allocated memory
            free(bits_11_5);
            free(bits_4_0);
            free(immediate_);
        }
        // Opcode B
        else if (strcmp(binary_opcode, "1100011") == 0) {
            // Calculate immediate value:
            char instruction_[32];
            to_binary_string(is_id.instruction, instruction_, 32);
            int size;

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
            if (strcmp(binary_funct3, "000") == 0) {
                strcpy(id_rf.instruction, "BEQ");
                id_rf.imm = immediate;
                sprintf(id_rf.status, "%s R%u, R%u, #%hi", id_rf.instruction, id_rf.rs1, id_rf.rs2, id_rf.imm);
                id_rf.rd = 0;
            }
            // BNE
            else if (strcmp(binary_funct3, "001") == 0) {
                strcpy(id_rf.instruction, "BNE");
                id_rf.imm = immediate;
                sprintf(id_rf.status, "%s R%u, R%u, #%hi", id_rf.instruction, id_rf.rs1, id_rf.rs2, id_rf.imm);
                id_rf.rd = 0;
            }
            // BLT
            else if (strcmp(binary_funct3, "100") == 0) {
                strcpy(id_rf.instruction, "BLT");
                id_rf.imm = immediate;
                sprintf(id_rf.status, "%s R%u, R%u, #%hi", id_rf.instruction, id_rf.rs1, id_rf.rs2, id_rf.imm);
                id_rf.rd = 0;
            }
            // BGE
            else if (strcmp(binary_funct3, "101") == 0) {
                strcpy(id_rf.instruction, "BGE");
                id_rf.imm = immediate;
                sprintf(id_rf.status, "%s R%u, R%u, #%hi", id_rf.instruction, id_rf.rs1, id_rf.rs2, id_rf.imm);
                id_rf.rd = 0;
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
        else if (strcmp(binary_opcode, "1100111") == 0) {
            // Calculate immediate value:
            char instruction_[32];
            to_binary_string(is_id.instruction, instruction_, 32);
            int size;

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
                if (strcmp(binary_rd, "00000") == 0 && strcmp(binary_rs1, "00001") == 0 && !immediate) {
                    strcpy(id_rf.instruction, "RET");
                    id_rf.imm = 0;
                    id_rf.rd = 0;
                    id_rf.rs1 = 0;
                    id_rf.rs2 = 0;
                    id_rf.returned = true;
                    CPUstate.break_pc = if_is.NPC - 4;
                    sprintf(id_rf.status, "%s", id_rf.instruction);
                }
                // JALR
                else {
                    strcpy(id_rf.instruction, "JALR");
                    id_rf.imm = immediate;
                    sprintf(id_rf.status, "%s R%u, R%u, #%hi", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.imm);
                    id_rf.rs2 = 0;
                }
            }

            // Free dynamically allocated memory
            free(bits_0_11);
            free(immediate_);
        }
        // Still Opcode I
        else if (strcmp(binary_opcode, "0000011") == 0) {
            // Calculate immediate value:
            char instruction_[32];
            to_binary_string(is_id.instruction, instruction_, 32);
            int size;

            char *bits_0_11 = processSlice(instruction_, 0, 12, &size);
            const char *opcode_i_slices[] = {bits_0_11};
            char *immediate_ = combineSlices(opcode_i_slices, 1);
            int immediate = (int)strtol(immediate_, NULL, 2);

            // Sign extend immediate
            if (immediate & 0x800) {
                    immediate |= 0xF000;
                }
                
            // LW
            if (strcmp(binary_funct3, "010") == 0) {
                strcpy(id_rf.instruction, "LW");
                id_rf.imm = immediate;
                sprintf(id_rf.status, "%s R%u, %d(R%u)", id_rf.instruction, id_rf.rd, id_rf.imm, id_rf.rs1);
                id_rf.rs2 = 0;
            }

            // Free dynamically allocated memory
            free(bits_0_11);
            free(immediate_);
        }
        // Still Opcode I
        else if (strcmp(binary_opcode, "0010011") == 0) {
            // Calculate immediate value:
            char instruction_[32];
            to_binary_string(is_id.instruction, instruction_, 32);
            int size;

            char *bits_0_11 = processSlice(instruction_, 0, 12, &size);
            const char *opcode_i_slices[] = {bits_0_11};
            char *immediate_ = combineSlices(opcode_i_slices, 1);
            int immediate = (int)strtol(immediate_, NULL, 2);

            // Sign extend immediate
            if (immediate & 0x800) {
                    immediate |= 0xF000;
                }
                
            // ADDI and NOP
            if (strcmp(binary_funct3, "000") == 0) {
                // NOP
                if (strcmp(binary_rd, "00000") == 0 && strcmp(binary_rs1, "00000") == 0 && !immediate) {
                    strcpy(id_rf.instruction, "NOP");
                    sprintf(id_rf.status, "%s", id_rf.instruction);
                    id_rf.imm = 0;
                    id_rf.rd = 0;
                    id_rf.rs1 = 0;
                    id_rf.rs2 = 0;
                }
                // ADDI
                else {
                    strcpy(id_rf.instruction, "ADDI");
                    id_rf.imm = immediate;
                    sprintf(id_rf.status, "%s R%u, R%u, #%hi", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.imm);
                    id_rf.rs2 = 0;
                }
            }
            // SLTI
            else if (strcmp(binary_funct3, "010") == 0) {
                strcpy(id_rf.instruction, "SLTI");
                id_rf.imm = immediate;
                sprintf(id_rf.status, "%s R%u, R%u, #%hi", id_rf.instruction, id_rf.rd, id_rf.rs1, id_rf.imm);
                id_rf.rs2 = 0;
            }

            // Free dynamically allocated memory
            free(bits_0_11);
            free(immediate_);

        }
        // Opcode J
        else if (strcmp(binary_opcode, "1101111") == 0) {
            // Calculate immediate value:
            char instruction_[32];
            to_binary_string(is_id.instruction, instruction_, 32);
            int size;

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
                    strcpy(id_rf.instruction, "J");
                    id_rf.imm = immediate;
                    sprintf(id_rf.status, "%s #520", id_rf.instruction);
                    id_rf.rd = 0;
                    id_rf.rs1 = 0;
                    id_rf.rs2 = 0;
                }
                // JAL
                else {
                    strcpy(id_rf.instruction, "JAL");
                    id_rf.imm = immediate;
                    sprintf(id_rf.status, "%s R%u, #%hi", id_rf.instruction, id_rf.rd, id_rf.imm);
                    id_rf.rs1 = 0;
                    id_rf.rs2 = 0;
                }
            }

            // Free dynamically allocated memory
            free(bits_20);
            free(bits_12_19);
            free(bits_11);
            free(bits_1_10);
            free(immediate_);
            free(shifted);
        } else {
            strcpy(id_rf.instruction, "NOP");
            sprintf(id_rf.status, "%s", id_rf.instruction);
            id_rf.imm = 0;
            id_rf.rd = 0;
            id_rf.rs1 = 0;
            id_rf.rs2 = 0;
        }
    }

    // Hazard checking
    //Load hazard stall

    if (strcmp(rf_ex.instruction, "LW") == 0) {
        if ((strcmp(id_rf.instruction, "ADD") == 0) || (strcmp(id_rf.instruction, "SUB") == 0) ||
            (strcmp(id_rf.instruction, "SLL") == 0) || (strcmp(id_rf.instruction, "SLT") == 0) ||
            (strcmp(id_rf.instruction, "SRL") == 0) || (strcmp(id_rf.instruction, "AND") == 0) ||
            (strcmp(id_rf.instruction, "OR") == 0) || (strcmp(id_rf.instruction, "XOR") == 0) ||
            (strcmp(id_rf.instruction, "LW") == 0) || (strcmp(id_rf.instruction, "SW") == 0) ||
            (strcmp(id_rf.instruction, "ADDI") == 0) || (strcmp(id_rf.instruction, "JALR") == 0) ||
            (strcmp(id_rf.instruction, "SLTI") == 0) || (strcmp(id_rf.instruction, "BEQ") == 0) ||
            (strcmp(id_rf.instruction, "BNE") == 0) || (strcmp(id_rf.instruction, "BLT") == 0) ||
            (strcmp(id_rf.instruction, "BEQ") == 0)) {
                if (rf_ex.rd == id_rf.rs1) {
                    CPUstate.load_stall_length = 2;
                    sprintf(CPUstate.stall_inst, "%s", id_rf.status);
                }
        }
        if ((strcmp(id_rf.instruction, "ADD") == 0) || (strcmp(id_rf.instruction, "SUB") == 0) ||
            (strcmp(id_rf.instruction, "SLL") == 0) || (strcmp(id_rf.instruction, "SLT") == 0) ||
            (strcmp(id_rf.instruction, "SRL") == 0) || (strcmp(id_rf.instruction, "AND") == 0) ||
            (strcmp(id_rf.instruction, "OR") == 0) || (strcmp(id_rf.instruction, "XOR") == 0) ||
            (strcmp(id_rf.instruction, "SW") == 0) || (strcmp(id_rf.instruction, "BEQ") == 0) ||
            (strcmp(id_rf.instruction, "BNE") == 0) || (strcmp(id_rf.instruction, "BLT") == 0) ||
            (strcmp(id_rf.instruction, "BEQ") == 0)) {
                if (rf_ex.rd == id_rf.rs2) {
                    CPUstate.load_stall_length = 3;
                    sprintf(CPUstate.stall_inst, "%s", id_rf.status);
                }
        }
    }

    if (CPUstate.load_stall_length > 0) {
        
        sprintf(CPUstate.forwarding_inst, "(none)");
        if (CPUstate.load_stall_length > 2) {
        }
        return;
    } else {
        sprintf(CPUstate.stall_inst, "(none)");
    }

    //Branch hazard stall
    // if ((strcmp(rf_ex.instruction, "BEQ") == 0) || (strcmp(rf_ex.instruction, "BNE") == 0) || (strcmp(rf_ex.instruction, "BLT") == 0) || (strcmp(rf_ex.instruction, "BGE") == 0)) {
    //     CPUstate.branch_stalls++;
    // }

    sprintf(CPUstate.forwarding_inst, "");

    // if (CPUstate.load_stall_length > 0) {
    //     sprintf(CPUstate.forwarding_inst, "(none)");
    //     return;
    // }

    // Forwarding Detection
    if ((strcmp(rf_ex.instruction, "ADD") == 0) || (strcmp(rf_ex.instruction, "SUB") == 0) || 
        (strcmp(rf_ex.instruction, "SLL") == 0) || (strcmp(rf_ex.instruction, "SLT") == 0) ||
        (strcmp(rf_ex.instruction, "SRL") == 0) || (strcmp(rf_ex.instruction, "AND") == 0) ||
        (strcmp(rf_ex.instruction, "OR") == 0) || (strcmp(rf_ex.instruction, "XOR") == 0) || 
        (strcmp(rf_ex.instruction, "ADDI") == 0) || (strcmp(rf_ex.instruction, "SLTI") == 0) ||
        (strcmp(rf_ex.instruction, "JAL") == 0) || (strcmp(rf_ex.instruction, "JALR") == 0)) {
            if ((strcmp(id_rf.instruction, "ADD") == 0) || (strcmp(id_rf.instruction, "SUB") == 0) || 
                (strcmp(id_rf.instruction, "SLL") == 0) || (strcmp(id_rf.instruction, "SLT") == 0) ||
                (strcmp(id_rf.instruction, "SRL") == 0) || (strcmp(id_rf.instruction, "AND") == 0) ||
                (strcmp(id_rf.instruction, "OR") == 0) || (strcmp(id_rf.instruction, "XOR") == 0) || 
                (strcmp(id_rf.instruction, "ADDI") == 0) || (strcmp(id_rf.instruction, "SLTI") == 0) ||
                (strcmp(id_rf.instruction, "JALR") == 0) || (strcmp(id_rf.instruction, "BEQ") == 0) || 
                (strcmp(id_rf.instruction, "BNE") == 0) || (strcmp(id_rf.instruction, "BLT") == 0) ||
                (strcmp(id_rf.instruction, "BGE") == 0) || (strcmp(id_rf.instruction, "SW") == 0) || 
                (strcmp(id_rf.instruction, "LW") == 0)) {
                    if (rf_ex.rd == id_rf.rs1) {
                        sprintf(CPUstate.forwarding_inst, "%s(%s) to (%s)\n", CPUstate.forwarding_inst, rf_ex.status, id_rf.status);
                    }
            }
            if ((strcmp(id_rf.instruction, "ADD") == 0) || (strcmp(id_rf.instruction, "SUB") == 0) || 
                (strcmp(id_rf.instruction, "SLL") == 0) || (strcmp(id_rf.instruction, "SLT") == 0) ||
                (strcmp(id_rf.instruction, "SRL") == 0) || (strcmp(id_rf.instruction, "AND") == 0) ||
                (strcmp(id_rf.instruction, "OR") == 0) || (strcmp(id_rf.instruction, "XOR") == 0) || 
                (strcmp(id_rf.instruction, "BEQ") == 0) || (strcmp(id_rf.instruction, "BNE") == 0) || 
                (strcmp(id_rf.instruction, "BLT") == 0) || (strcmp(id_rf.instruction, "BGE") == 0)) {
                    if (rf_ex.rd == id_rf.rs2) {
                        sprintf(CPUstate.forwarding_inst, "%s(%s) to (%s)\n", CPUstate.forwarding_inst, rf_ex.status, id_rf.status);
                    }
            }
    }
    if ((strcmp(rf_ex.instruction, "ADD") == 0) || (strcmp(rf_ex.instruction, "SUB") == 0) || 
        (strcmp(rf_ex.instruction, "SLL") == 0) || (strcmp(rf_ex.instruction, "SLT") == 0) || 
        (strcmp(rf_ex.instruction, "SRL") == 0) || (strcmp(rf_ex.instruction, "AND") == 0) || 
        (strcmp(rf_ex.instruction, "OR") == 0) || (strcmp(rf_ex.instruction, "XOR") == 0) || 
        (strcmp(rf_ex.instruction, "ADDI") == 0) || (strcmp(rf_ex.instruction, "SLTI") == 0) || 
        (strcmp(rf_ex.instruction, "LW") == 0) || (strcmp(rf_ex.instruction, "JAL") == 0) || 
        (strcmp(rf_ex.instruction, "JALR") == 0)) {
            if ((strcmp(id_rf.instruction, "SW") == 0)) {
                if (rf_ex.rd == id_rf.rs2) {
                        sprintf(CPUstate.forwarding_inst, "%s(%s) to (%s)\n", CPUstate.forwarding_inst, rf_ex.status, id_rf.status);
                }
            }
    }

    if ((strcmp(ex_df.instruction, "ADD") == 0) || (strcmp(ex_df.instruction, "SUB") == 0) || 
        (strcmp(ex_df.instruction, "SLL") == 0) || (strcmp(ex_df.instruction, "SLT") == 0) || 
        (strcmp(ex_df.instruction, "SRL") == 0) || (strcmp(ex_df.instruction, "AND") == 0) || 
        (strcmp(ex_df.instruction, "OR") == 0) || (strcmp(ex_df.instruction, "XOR") == 0) || 
        (strcmp(ex_df.instruction, "ADDI") == 0) || (strcmp(ex_df.instruction, "SLTI") == 0) || 
        (strcmp(ex_df.instruction, "LW") == 0) || (strcmp(ex_df.instruction, "JAL") == 0) || 
        (strcmp(ex_df.instruction, "JALR") == 0)) {
            if ((strcmp(id_rf.instruction, "ADD") == 0) || (strcmp(id_rf.instruction, "SUB") == 0) || 
                (strcmp(id_rf.instruction, "SLL") == 0) || (strcmp(id_rf.instruction, "SLT") == 0) ||
                (strcmp(id_rf.instruction, "SRL") == 0) || (strcmp(id_rf.instruction, "AND") == 0) ||
                (strcmp(id_rf.instruction, "OR") == 0) || (strcmp(id_rf.instruction, "XOR") == 0) || 
                (strcmp(id_rf.instruction, "ADDI") == 0) || (strcmp(id_rf.instruction, "SLTI") == 0) ||
                (strcmp(id_rf.instruction, "JALR") == 0) || (strcmp(id_rf.instruction, "BEQ") == 0) || 
                (strcmp(id_rf.instruction, "BNE") == 0) || (strcmp(id_rf.instruction, "BLT") == 0) ||
                (strcmp(id_rf.instruction, "BGE") == 0) || (strcmp(id_rf.instruction, "SW") == 0) || 
                (strcmp(id_rf.instruction, "LW") == 0)) {
                    if (ex_df.rd == id_rf.rs1) {
                        sprintf(CPUstate.forwarding_inst, "%s(%s) to (%s)\n", CPUstate.forwarding_inst, ex_df.status, id_rf.status);
                    }
                    
            }
            if ((strcmp(id_rf.instruction, "ADD") == 0) || (strcmp(id_rf.instruction, "SUB") == 0) || 
                (strcmp(id_rf.instruction, "SLL") == 0) || (strcmp(id_rf.instruction, "SLT") == 0) ||
                (strcmp(id_rf.instruction, "SRL") == 0) || (strcmp(id_rf.instruction, "AND") == 0) ||
                (strcmp(id_rf.instruction, "OR") == 0) || (strcmp(id_rf.instruction, "XOR") == 0) || 
                (strcmp(id_rf.instruction, "BEQ") == 0) || (strcmp(id_rf.instruction, "BNE") == 0) || 
                (strcmp(id_rf.instruction, "BLT") == 0) || (strcmp(id_rf.instruction, "BGE") == 0) || 
                (strcmp(id_rf.instruction, "SW") == 0)) {
                    if (ex_df.rd == rf_ex.rs1) {
                    sprintf(CPUstate.forwarding_inst, "%s(%s) to (%s)\n", CPUstate.forwarding_inst, ex_df.status, id_rf.status);
                }
            }
    }

    if ((strcmp(df_ds.instruction, "ADD") == 0) || (strcmp(df_ds.instruction, "SUB") == 0) || 
        (strcmp(df_ds.instruction, "SLL") == 0) || (strcmp(df_ds.instruction, "SLT") == 0) || 
        (strcmp(df_ds.instruction, "SRL") == 0) || (strcmp(df_ds.instruction, "AND") == 0) || 
        (strcmp(df_ds.instruction, "OR") == 0) || (strcmp(df_ds.instruction, "XOR") == 0) || 
        (strcmp(df_ds.instruction, "ADDI") == 0) || (strcmp(df_ds.instruction, "SLTI") == 0) || 
        (strcmp(df_ds.instruction, "LW") == 0) || (strcmp(df_ds.instruction, "JAL") == 0) || 
        (strcmp(df_ds.instruction, "JALR") == 0)) {
            if ((strcmp(id_rf.instruction, "ADD") == 0) || (strcmp(id_rf.instruction, "SUB") == 0) || 
                (strcmp(id_rf.instruction, "SLL") == 0) || (strcmp(id_rf.instruction, "SLT") == 0) ||
                (strcmp(id_rf.instruction, "SRL") == 0) || (strcmp(id_rf.instruction, "AND") == 0) ||
                (strcmp(id_rf.instruction, "OR") == 0) || (strcmp(id_rf.instruction, "XOR") == 0) || 
                (strcmp(id_rf.instruction, "ADDI") == 0) || (strcmp(id_rf.instruction, "SLTI") == 0) ||
                (strcmp(id_rf.instruction, "JALR") == 0) || (strcmp(id_rf.instruction, "BEQ") == 0) || 
                (strcmp(id_rf.instruction, "BNE") == 0) || (strcmp(id_rf.instruction, "BLT") == 0) ||
                (strcmp(id_rf.instruction, "BGE") == 0) || (strcmp(id_rf.instruction, "SW") == 0) || 
                (strcmp(id_rf.instruction, "LW") == 0)) {
                    if (df_ds.rd == id_rf.rs1) {
                        sprintf(CPUstate.forwarding_inst, "%s(%s) to (%s)\n", CPUstate.forwarding_inst, df_ds.status, id_rf.status);
                    }
                    
            }
            if ((strcmp(id_rf.instruction, "ADD") == 0) || (strcmp(id_rf.instruction, "SUB") == 0) || 
                    (strcmp(id_rf.instruction, "SLL") == 0) || (strcmp(id_rf.instruction, "SLT") == 0) ||
                    (strcmp(id_rf.instruction, "SRL") == 0) || (strcmp(id_rf.instruction, "AND") == 0) ||
                    (strcmp(id_rf.instruction, "OR") == 0) || (strcmp(id_rf.instruction, "XOR") == 0) || 
                    (strcmp(id_rf.instruction, "BEQ") == 0) || (strcmp(id_rf.instruction, "BNE") == 0) || 
                    (strcmp(id_rf.instruction, "BLT") == 0) || (strcmp(id_rf.instruction, "BGE") == 0) || 
                    (strcmp(id_rf.instruction, "SW") == 0)) {
                        if (df_ds.rd == id_rf.rs2) {
                            sprintf(CPUstate.forwarding_inst, "%s(%s) to (%s)\n", CPUstate.forwarding_inst, df_ds.status, id_rf.status);
                        }
            }
    }

    if ((strcmp(ex_df.instruction, "ADD") == 0) || (strcmp(ex_df.instruction, "SUB") == 0) || 
        (strcmp(ex_df.instruction, "SLL") == 0) || (strcmp(ex_df.instruction, "SLT") == 0) || 
        (strcmp(ex_df.instruction, "SRL") == 0) || (strcmp(ex_df.instruction, "AND") == 0) || 
        (strcmp(ex_df.instruction, "OR") == 0) || (strcmp(ex_df.instruction, "XOR") == 0) || 
        (strcmp(ex_df.instruction, "ADDI") == 0) || (strcmp(ex_df.instruction, "SLTI") == 0) || 
        (strcmp(ex_df.instruction, "LW") == 0) || (strcmp(ex_df.instruction, "JAL") == 0) || 
        (strcmp(ex_df.instruction, "JALR") == 0)) {
            if ((strcmp(id_rf.instruction, "SW") == 0)) {
                if (ex_df.rd == id_rf.rs2) {
                            sprintf(CPUstate.forwarding_inst, "%s(%s) to (%s)\n", CPUstate.forwarding_inst, ex_df.status, id_rf.status);
                }
            }
    }

    if (strcmp(CPUstate.forwarding_inst, "") == 0) {
        sprintf(CPUstate.forwarding_inst, "(none)");
    }
};

void RF() {
    if ((CPUstate.load_stall_length > 0) || ex_df.branch_taken) {
        sprintf(rf_ex.status, "**STALL**");
        sprintf(rf_ex.instruction, "**STALL**");
        rf_ex.rs1_data = 0;
        rf_ex.rs2_data = 0;
        rf_ex.rd = 0;
        rf_ex.rs1 = 0;
        rf_ex.rs2 = 0;
        rf_ex.imm = 0;

    } else {
        rf_ex.stalled = false;
        rf_ex.rs1_data = register_file[id_rf.rs1];
        rf_ex.rs2_data = register_file[id_rf.rs2];
        rf_ex.rd = id_rf.rd;
        rf_ex.rs1 = id_rf.rs1;
        rf_ex.rs2 = id_rf.rs2;
        rf_ex.imm = id_rf.imm;
        strcpy(rf_ex.instruction, id_rf.instruction);
        strcpy(rf_ex.status, id_rf.status);
    }
};

void EX() {
    int alu_result = 0;
    uint32_t branch_target = 0;
    bool branch_taken = false;

    if (strcmp(rf_ex.instruction, "ADD") == 0) {
        alu_result = rf_ex.rs1_data + rf_ex.rs2_data;
    }
    else if (strcmp(rf_ex.instruction, "SUB") == 0) {
        alu_result = rf_ex.rs1_data - rf_ex.rs2_data;
    }
    else if (strcmp(rf_ex.instruction, "AND") == 0) {
        alu_result = rf_ex.rs1_data & rf_ex.rs2_data;
    }
    else if (strcmp(rf_ex.instruction, "OR") == 0) {
        alu_result = rf_ex.rs1_data | rf_ex.rs2_data;
    }
    else if (strcmp(rf_ex.instruction, "XOR") == 0) {
        alu_result = rf_ex.rs1_data ^ rf_ex.rs2_data;
    }
    else if (strcmp(rf_ex.instruction, "SLT") == 0) {
        alu_result = (rf_ex.rs1_data < rf_ex.rs2_data) ? 1 : 0;
    }
    else if (strcmp(rf_ex.instruction, "SLL") == 0) {
        alu_result = rf_ex.rs1_data << rf_ex.rs2_data;
    }
    else if (strcmp(rf_ex.instruction, "SRL") == 0) {
        alu_result = rf_ex.rs1_data >> rf_ex.rs2_data;
    }
    else if (strcmp(rf_ex.instruction, "ADDI") == 0) {
        alu_result = rf_ex.rs1_data + rf_ex.imm;
    }
    else if (strcmp(rf_ex.instruction, "LW") == 0 || strcmp(rf_ex.instruction, "SW") == 0) {
        alu_result = rf_ex.rs1_data + rf_ex.imm;
    }
    else if (strcmp(rf_ex.instruction, "BEQ") == 0) {
        if (rf_ex.rs1_data == rf_ex.rs2_data) {
            branch_taken = true;
            branch_target = rf_ex.imm + if_is.NPC;  // NPC + immediate for branch target
            
        }
    }
    else if (strcmp(rf_ex.instruction, "BNE") == 0) {
        // For BNE, evaluate the condition and compute the branch target
        if (rf_ex.rs1_data != rf_ex.rs2_data) {
            branch_taken = true;
            branch_target = rf_ex.imm + if_is.NPC;
        }
    }
    else if (strcmp(rf_ex.instruction, "BGE") == 0) {
        if (rf_ex.rs1_data >= rf_ex.rs2_data) {
            branch_taken = true;
            branch_target = rf_ex.imm + if_is.NPC;  // NPC + immediate for branch target
        }
    }
    else if (strcmp(rf_ex.instruction, "BLT") == 0) {
        if (rf_ex.rs1_data < rf_ex.rs2_data) {
            branch_taken = true;
            branch_target = rf_ex.imm + if_is.NPC;  // NPC + immediate for branch target
        }
    }
    else if (strcmp(rf_ex.instruction, "JAL") == 0) {
        branch_taken = true;
        branch_target = rf_ex.imm + if_is.NPC;  // NPC + immediate for jump target
    }
    else if (strcmp(rf_ex.instruction, "JALR") == 0) {
        branch_taken = true;
        branch_target = (rf_ex.rs1_data + rf_ex.imm) & ~1;  // Ensure the target is aligned (clear least significant bit)
    }
    else if (strcmp(rf_ex.instruction, "J") == 0) {
        branch_taken = true;
        branch_target = 520;  // Jump target is NPC + immediate
    }

    ex_df.alu_result = alu_result;
    ex_df.branch_taken = branch_taken;
    
    ex_df.rs1 = rf_ex.rs1;
    ex_df.rs2 = rf_ex.rs2;
    ex_df.rd = rf_ex.rd;
    ex_df.imm = rf_ex.imm;
    ex_df.rs1_data = rf_ex.rs1_data;
    ex_df.rs2_data = rf_ex.rs2_data;
    ex_df.rs1_forwarded = rf_ex.rs1_forwarded;
    ex_df.rs2_forwarded = rf_ex.rs2_forwarded;
    strcpy(ex_df.instruction, rf_ex.instruction);
    strcpy(ex_df.status, rf_ex.status);

    ex_df.mem_read = false;
    ex_df.mem_write = false;

    if(ex_df.branch_taken) {
        ex_df.branch_target = branch_target;
    }
};

void DF() {
    if (strcmp(ex_df.instruction, "LW") == 0) {
        df_ds.mem_address = ex_df.alu_result;
        df_ds.mem_read = true;
        if ((df_ds.mem_address >= 600) && (df_ds.mem_address <= 636)) {
            df_ds.mem_data = data_memory[(df_ds.mem_address - 600) / 4];
        }
    } else if (strcmp(ex_df.instruction, "SW") == 0) {
        df_ds.mem_address = ex_df.alu_result;
        df_ds.mem_write = true;
        df_ds.mem_data = ex_df.rs2_data;
    }

    df_ds.rd = ex_df.rd;
    df_ds.rs1 = ex_df.rs1;
    df_ds.rs2 = ex_df.rs2;
    df_ds.imm = ex_df.imm;
    df_ds.alu_result = ex_df.alu_result;
    strcpy(df_ds.instruction, ex_df.instruction);
    strcpy(df_ds.status, ex_df.status);
};

void DS() {
    if (strcmp(df_ds.instruction, "LW") == 0) {
        // Since the data is already fetched in DF stage, we just forward the data here
        // Data is ready to be forwarded for write-back (or to be used in the next stages)
        ds_wb.mem_data = df_ds.mem_data;
    }
    else if (strcmp(df_ds.instruction, "SW") == 0) {
        if ((df_ds.mem_address >= 600) && (df_ds.mem_address <= 636)) {
            data_memory[(df_ds.mem_address - 600) / 4] = df_ds.mem_data;
            ds_wb.mem_data = df_ds.mem_address;
        }
    }

    ds_wb.rd = df_ds.rd;
    ds_wb.rs1 = df_ds.rs1;
    ds_wb.rs2 = df_ds.rs2;
    ds_wb.imm = df_ds.imm;
    ds_wb.alu_result = df_ds.alu_result;
    strcpy(ds_wb.instruction, df_ds.instruction);
    strcpy(ds_wb.status, df_ds.status);
};

void WB() {
    if (strcmp(ds_wb.instruction, "LW") == 0) {
        register_file[ds_wb.rd] = ds_wb.mem_data;
    }
    if (strcmp(ds_wb.instruction, "ADD") == 0 || strcmp(ds_wb.instruction, "SUB") == 0 || 
             strcmp(ds_wb.instruction, "AND") == 0 || strcmp(ds_wb.instruction, "OR") == 0 || 
             strcmp(ds_wb.instruction, "XOR") == 0 || strcmp(ds_wb.instruction, "SLT") == 0 ||
             strcmp(ds_wb.instruction, "SLL") == 0 || strcmp(ds_wb.instruction, "SRL") == 0 ||
             strcmp(ds_wb.instruction, "JALR") == 0 || strcmp(ds_wb.instruction, "ADDI") == 0 ||
             strcmp(ds_wb.instruction, "SLTI") == 0 || strcmp(ds_wb.instruction, "JAL") == 0) {

        register_file[ds_wb.rd] = ds_wb.alu_result;
    }

    if (strcmp(ds_wb.instruction, "RET") == 0) {
        CPUstate.returned = true;
    }

    wb.rd = ds_wb.rd;
    wb.rs1 = ds_wb.rs1;
    wb.rs2 = ds_wb.rs2;
    wb.imm = ds_wb.imm;
    strcpy(wb.instruction, ds_wb.instruction);
    strcpy(wb.status, ds_wb.status);
};

void initialize(FILE* input_file) {
    strcpy(is_id._instruction, "NOP");
    strcpy(id_rf.instruction, "NOP");
    strcpy(rf_ex.instruction, "NOP");
    strcpy(ex_df.instruction, "NOP");
    strcpy(df_ds.instruction, "NOP");
    strcpy(ds_wb.instruction, "NOP");
    strcpy(wb.instruction, "NOP");

    strcpy(if_is.status, "<unknown>");
    strcpy(is_id.status, "NOP");
    strcpy(id_rf.status, "NOP");
    strcpy(rf_ex.status, "NOP");
    strcpy(ex_df.status, "NOP");
    strcpy(df_ds.status, "NOP");
    strcpy(ds_wb.status, "NOP");
    strcpy(wb.status, "NOP");
    if_is.init = true; 

    strcpy(CPUstate.stall_inst, "(none)");
    CPUstate.load_stall = false;
    CPUstate.load_stall_new = false;

    strcpy(CPUstate.ex_df_to_rf_ex, "(none)");
    strcpy(CPUstate.df_ds_to_ex_df, "(none)");
    strcpy(CPUstate.df_ds_to_rf_ex, "(none)");
    strcpy(CPUstate.ds_wb_to_ex_df, "(none)");
    strcpy(CPUstate.ds_wb_to_rf_ex, "(none)");

    strcpy(CPUstate.ex_df_to_rf_ex_out, "(none)");
    strcpy(CPUstate.df_ds_to_ex_df_out, "(none)");
    strcpy(CPUstate.df_ds_to_rf_ex_out, "(none)");
    strcpy(CPUstate.ds_wb_to_ex_df_out, "(none)");
    strcpy(CPUstate.ds_wb_to_rf_ex_out, "(none)");

    int i;

    for (i = 0; i < 10; i++) {
        data_memory[i] = readSpecificLine(input_file, 600 + i * 4);
    }
    
    if_is.NPC = 496;

    if_is.input_file = input_file;
    id_rf.returned = false;

    CPUstate.cycle = 0;
};

void handle_forward() {
    // if (strcmp(CPUstate.ex_df_to_rf_ex, "(none)")) {
    //     CPUstate._ex_df_to_rf_ex++;
    // }

    // if (strcmp(CPUstate.df_ds_to_ex_df, "(none)")) {
    //     CPUstate._df_ds_to_ex_df++;
    // }

    // if (strcmp(CPUstate.df_ds_to_rf_ex, "(none)")) {
    //     CPUstate._df_ds_to_rf_ex++;
    // }

    // if (strcmp(CPUstate.ds_wb_to_rf_ex, "(none)")) {
    //     CPUstate._ds_wb_to_rf_ex++;
    // }

    // if (strcmp(CPUstate.ds_wb_to_ex_df, "(none)")) {
    //     CPUstate._ds_wb_to_ex_df++;
    // }

    // strcpy(CPUstate.ex_df_to_rf_ex_out, CPUstate.ex_df_to_rf_ex);
    // strcpy(CPUstate.df_ds_to_ex_df_out, CPUstate.df_ds_to_ex_df);
    // strcpy(CPUstate.df_ds_to_rf_ex_out, CPUstate.df_ds_to_rf_ex);
    // strcpy(CPUstate.ds_wb_to_rf_ex_out, CPUstate.ds_wb_to_rf_ex);
    // strcpy(CPUstate.ds_wb_to_ex_df_out, CPUstate.ds_wb_to_ex_df);

    strcpy(CPUstate.ex_df_to_rf_ex, "(none)");
    strcpy(CPUstate.df_ds_to_ex_df, "(none)");
    strcpy(CPUstate.df_ds_to_rf_ex, "(none)");
    strcpy(CPUstate.ds_wb_to_ex_df, "(none)");
    strcpy(CPUstate.ds_wb_to_rf_ex, "(none)");

    rf_ex.rs1_forwarded = false;
    rf_ex.rs2_forwarded = false;

    ex_df_to_rf_ex.data = ex_df.alu_result;
    df_ds_to_ex_df.data = df_ds.alu_result;
    df_ds_to_rf_ex.data = df_ds.mem_data;
    ds_wb_to_rf_ex.data = ds_wb.alu_result;
    ds_wb_to_ex_df.data = ds_wb.mem_data;

    if ((strcmp(ex_df.instruction, "ADD") == 0) || (strcmp(ex_df.instruction, "SUB") == 0) || 
        (strcmp(ex_df.instruction, "SLL") == 0) || (strcmp(ex_df.instruction, "SLT") == 0) ||
        (strcmp(ex_df.instruction, "SRL") == 0) || (strcmp(ex_df.instruction, "AND") == 0) ||
        (strcmp(ex_df.instruction, "OR") == 0) || (strcmp(ex_df.instruction, "XOR") == 0) || 
        (strcmp(ex_df.instruction, "ADDI") == 0) || (strcmp(ex_df.instruction, "SLTI") == 0) ||
        (strcmp(ex_df.instruction, "JAL") == 0) || (strcmp(ex_df.instruction, "JALR") == 0)) {
            if ((strcmp(rf_ex.instruction, "ADD") == 0) || (strcmp(rf_ex.instruction, "SUB") == 0) || 
                (strcmp(rf_ex.instruction, "SLL") == 0) || (strcmp(rf_ex.instruction, "SLT") == 0) ||
                (strcmp(rf_ex.instruction, "SRL") == 0) || (strcmp(rf_ex.instruction, "AND") == 0) ||
                (strcmp(rf_ex.instruction, "OR") == 0) || (strcmp(rf_ex.instruction, "XOR") == 0) || 
                (strcmp(rf_ex.instruction, "ADDI") == 0) || (strcmp(rf_ex.instruction, "SLTI") == 0) ||
                (strcmp(rf_ex.instruction, "JALR") == 0) || (strcmp(rf_ex.instruction, "BEQ") == 0) || 
                (strcmp(rf_ex.instruction, "BNE") == 0) || (strcmp(rf_ex.instruction, "BLT") == 0) ||
                (strcmp(rf_ex.instruction, "BGE") == 0) || (strcmp(rf_ex.instruction, "SW") == 0) || 
                (strcmp(rf_ex.instruction, "LW") == 0)) {
                    if (ex_df.rd == rf_ex.rs1) {
                        rf_ex.rs1_data = ex_df_to_rf_ex.data;
                        rf_ex.rs1_forwarded = true;
                        sprintf(CPUstate.ex_df_to_rf_ex, "(%s) to (%s)", ex_df.status, rf_ex.status);
                        CPUstate._ex_df_to_rf_ex++;
                    } 
            }
            if ((strcmp(rf_ex.instruction, "ADD") == 0) || (strcmp(rf_ex.instruction, "SUB") == 0) || 
                (strcmp(rf_ex.instruction, "SLL") == 0) || (strcmp(rf_ex.instruction, "SLT") == 0) ||
                (strcmp(rf_ex.instruction, "SRL") == 0) || (strcmp(rf_ex.instruction, "AND") == 0) ||
                (strcmp(rf_ex.instruction, "OR") == 0) || (strcmp(rf_ex.instruction, "XOR") == 0) || 
                (strcmp(rf_ex.instruction, "BEQ") == 0) || (strcmp(rf_ex.instruction, "BNE") == 0) || 
                (strcmp(rf_ex.instruction, "BLT") == 0) || (strcmp(rf_ex.instruction, "BGE") == 0)) {
                    if (ex_df.rd == rf_ex.rs2) {
                        rf_ex.rs2_data = ex_df_to_rf_ex.data;
                        rf_ex.rs2_forwarded = true;
                        sprintf(CPUstate.ex_df_to_rf_ex, "(%s) to (%s)", ex_df.status, rf_ex.status);
                        CPUstate._ex_df_to_rf_ex++;
                    }
            }
    }

    if ((strcmp(df_ds.instruction, "ADD") == 0) || (strcmp(df_ds.instruction, "SUB") == 0) || 
        (strcmp(df_ds.instruction, "SLL") == 0) || (strcmp(df_ds.instruction, "SLT") == 0) || 
        (strcmp(df_ds.instruction, "SRL") == 0) || (strcmp(df_ds.instruction, "AND") == 0) || 
        (strcmp(df_ds.instruction, "OR") == 0) || (strcmp(df_ds.instruction, "XOR") == 0) || 
        (strcmp(df_ds.instruction, "ADDI") == 0) || (strcmp(df_ds.instruction, "SLTI") == 0) || 
        (strcmp(df_ds.instruction, "LW") == 0) || (strcmp(df_ds.instruction, "JAL") == 0) || 
        (strcmp(df_ds.instruction, "JALR") == 0)) {
            if ((strcmp(ex_df.instruction, "SW") == 0)) {
                if ((df_ds.rd == ex_df.rs2) && !ex_df.rs2_forwarded) {
                    ex_df.rs2_data = df_ds_to_ex_df.data;
                    sprintf(CPUstate.df_ds_to_ex_df, "(%s) to (%s)", df_ds.status, ex_df.status);
                    CPUstate._df_ds_to_ex_df++;
                }
            }
    }

    if ((strcmp(df_ds.instruction, "ADD") == 0) || (strcmp(df_ds.instruction, "SUB") == 0) || 
        (strcmp(df_ds.instruction, "SLL") == 0) || (strcmp(df_ds.instruction, "SLT") == 0) || 
        (strcmp(df_ds.instruction, "SRL") == 0) || (strcmp(df_ds.instruction, "AND") == 0) || 
        (strcmp(df_ds.instruction, "OR") == 0) || (strcmp(df_ds.instruction, "XOR") == 0) || 
        (strcmp(df_ds.instruction, "ADDI") == 0) || (strcmp(df_ds.instruction, "SLTI") == 0) || 
        (strcmp(df_ds.instruction, "LW") == 0) || (strcmp(df_ds.instruction, "JAL") == 0) || 
        (strcmp(df_ds.instruction, "JALR") == 0)) {
            if ((strcmp(rf_ex.instruction, "ADD") == 0) || (strcmp(rf_ex.instruction, "SUB") == 0) || 
                (strcmp(rf_ex.instruction, "SLL") == 0) || (strcmp(rf_ex.instruction, "SLT") == 0) ||
                (strcmp(rf_ex.instruction, "SRL") == 0) || (strcmp(rf_ex.instruction, "AND") == 0) ||
                (strcmp(rf_ex.instruction, "OR") == 0) || (strcmp(rf_ex.instruction, "XOR") == 0) || 
                (strcmp(rf_ex.instruction, "ADDI") == 0) || (strcmp(rf_ex.instruction, "SLTI") == 0) ||
                (strcmp(rf_ex.instruction, "JALR") == 0) || (strcmp(rf_ex.instruction, "BEQ") == 0) || 
                (strcmp(rf_ex.instruction, "BNE") == 0) || (strcmp(rf_ex.instruction, "BLT") == 0) ||
                (strcmp(rf_ex.instruction, "BGE") == 0) || (strcmp(rf_ex.instruction, "SW") == 0) || 
                (strcmp(rf_ex.instruction, "LW") == 0)) {
                    if ((df_ds.rd == rf_ex.rs1) && !rf_ex.rs1_forwarded) {
                        rf_ex.rs1_data = df_ds_to_rf_ex.data;
                        rf_ex.rs1_forwarded = true;
                        sprintf(CPUstate.df_ds_to_rf_ex, "(%s) to (%s)", df_ds.status, rf_ex.status);
                        CPUstate._df_ds_to_rf_ex++;
                    }
                    
            }
            if ((strcmp(rf_ex.instruction, "ADD") == 0) || (strcmp(rf_ex.instruction, "SUB") == 0) || 
                (strcmp(rf_ex.instruction, "SLL") == 0) || (strcmp(rf_ex.instruction, "SLT") == 0) ||
                (strcmp(rf_ex.instruction, "SRL") == 0) || (strcmp(rf_ex.instruction, "AND") == 0) ||
                (strcmp(rf_ex.instruction, "OR") == 0) || (strcmp(rf_ex.instruction, "XOR") == 0) || 
                (strcmp(rf_ex.instruction, "BEQ") == 0) || (strcmp(rf_ex.instruction, "BNE") == 0) || 
                (strcmp(rf_ex.instruction, "BLT") == 0) || (strcmp(rf_ex.instruction, "BGE") == 0) || 
                (strcmp(rf_ex.instruction, "SW") == 0)) {
                    if ((df_ds.rd == rf_ex.rs2) && !rf_ex.rs2_forwarded) {
                        rf_ex.rs2_data = df_ds_to_rf_ex.data;
                        rf_ex.rs2_forwarded = true;
                        sprintf(CPUstate.df_ds_to_rf_ex, "(%s) to (%s)", df_ds.status, rf_ex.status);
                        CPUstate._df_ds_to_rf_ex++;
                    }
            }
    }
    
    if ((strcmp(ds_wb.instruction, "ADD") == 0) || (strcmp(ds_wb.instruction, "SUB") == 0) || 
        (strcmp(ds_wb.instruction, "SLL") == 0) || (strcmp(ds_wb.instruction, "SLT") == 0) || 
        (strcmp(ds_wb.instruction, "SRL") == 0) || (strcmp(ds_wb.instruction, "AND") == 0) || 
        (strcmp(ds_wb.instruction, "OR") == 0) || (strcmp(ds_wb.instruction, "XOR") == 0) || 
        (strcmp(ds_wb.instruction, "ADDI") == 0) || (strcmp(ds_wb.instruction, "SLTI") == 0) || 
        (strcmp(ds_wb.instruction, "LW") == 0) || (strcmp(ds_wb.instruction, "JAL") == 0) || 
        (strcmp(ds_wb.instruction, "JALR") == 0)) {
            if ((strcmp(rf_ex.instruction, "ADD") == 0) || (strcmp(rf_ex.instruction, "SUB") == 0) || 
                (strcmp(rf_ex.instruction, "SLL") == 0) || (strcmp(rf_ex.instruction, "SLT") == 0) ||
                (strcmp(rf_ex.instruction, "SRL") == 0) || (strcmp(rf_ex.instruction, "AND") == 0) ||
                (strcmp(rf_ex.instruction, "OR") == 0) || (strcmp(rf_ex.instruction, "XOR") == 0) || 
                (strcmp(rf_ex.instruction, "ADDI") == 0) || (strcmp(rf_ex.instruction, "SLTI") == 0) ||
                (strcmp(rf_ex.instruction, "JALR") == 0) || (strcmp(rf_ex.instruction, "BEQ") == 0) || 
                (strcmp(rf_ex.instruction, "BNE") == 0) || (strcmp(rf_ex.instruction, "BLT") == 0) ||
                (strcmp(rf_ex.instruction, "BGE") == 0) || (strcmp(rf_ex.instruction, "SW") == 0) || 
                (strcmp(rf_ex.instruction, "LW") == 0)) {
                    if ((ds_wb.rd == rf_ex.rs1) && !rf_ex.rs1_forwarded) {
                        if (strcmp(ds_wb.instruction, "LW") == 0) {
                            rf_ex.rs1_data = ds_wb.mem_data;
                            rf_ex.rs1_forwarded = true;
                            sprintf(CPUstate.ds_wb_to_rf_ex, "(%s) to (%s)", ds_wb.status, rf_ex.status);
                            CPUstate._ds_wb_to_rf_ex++;
                        } else {
                            rf_ex.rs1_data = ds_wb_to_rf_ex.data;
                            rf_ex.rs1_forwarded = true;
                            sprintf(CPUstate.ds_wb_to_rf_ex, "(%s) to (%s)", ds_wb.status, rf_ex.status);
                            CPUstate._ds_wb_to_rf_ex++;
                        }
                    }
                    
            }
            if ((strcmp(rf_ex.instruction, "ADD") == 0) || (strcmp(rf_ex.instruction, "SUB") == 0) || 
                (strcmp(rf_ex.instruction, "SLL") == 0) || (strcmp(rf_ex.instruction, "SLT") == 0) ||
                (strcmp(rf_ex.instruction, "SRL") == 0) || (strcmp(rf_ex.instruction, "AND") == 0) ||
                (strcmp(rf_ex.instruction, "OR") == 0) || (strcmp(rf_ex.instruction, "XOR") == 0) || 
                (strcmp(rf_ex.instruction, "BEQ") == 0) || (strcmp(rf_ex.instruction, "BNE") == 0) || 
                (strcmp(rf_ex.instruction, "BLT") == 0) || (strcmp(rf_ex.instruction, "BGE") == 0) || (strcmp(rf_ex.instruction, "SW") == 0)) {
                    if ((ds_wb.rd == rf_ex.rs2) && !rf_ex.rs2_forwarded) {
                        if (strcmp(ds_wb.instruction, "LW") == 0) {
                            rf_ex.rs2_data = ds_wb.mem_data;
                            rf_ex.rs2_forwarded = true;
                            sprintf(CPUstate.ds_wb_to_rf_ex, "(%s) to (%s)", ds_wb.status, rf_ex.status);
                            CPUstate._ds_wb_to_rf_ex++;
                        } else {
                            rf_ex.rs2_data = ds_wb_to_rf_ex.data;
                            rf_ex.rs2_forwarded = true;
                            sprintf(CPUstate.ds_wb_to_rf_ex, "(%s) to (%s)", ds_wb.status, rf_ex.status);
                            CPUstate._ds_wb_to_rf_ex++;
                        }
                    }
            }
    }

    if ((strcmp(ds_wb.instruction, "ADD") == 0) || (strcmp(ds_wb.instruction, "SUB") == 0) || 
        (strcmp(ds_wb.instruction, "SLL") == 0) || (strcmp(ds_wb.instruction, "SLT") == 0) || 
        (strcmp(ds_wb.instruction, "SRL") == 0) || (strcmp(ds_wb.instruction, "AND") == 0) || 
        (strcmp(ds_wb.instruction, "OR") == 0) || (strcmp(ds_wb.instruction, "XOR") == 0) || 
        (strcmp(ds_wb.instruction, "ADDI") == 0) || (strcmp(ds_wb.instruction, "SLTI") == 0) || 
        (strcmp(ds_wb.instruction, "LW") == 0) || (strcmp(ds_wb.instruction, "JAL") == 0) || 
        (strcmp(ds_wb.instruction, "JALR") == 0)) {
            if ((strcmp(ex_df.instruction, "SW") == 0)) {
                if ((ds_wb.rd == ex_df.rs2) && !ex_df.rs2_forwarded) {
                    ex_df.rs2_data = ds_wb_to_ex_df.data;
                    sprintf(CPUstate.ds_wb_to_ex_df, "(%s) to (%s)", ds_wb.status, ex_df.status);
                    CPUstate._ds_wb_to_ex_df++;
                }
            }
    } 
};

void trace_output(FILE* output_file) {
    int i;
    int j;

    fprintf(output_file, "***** Cycle #%i ***********************************************\n", CPUstate.cycle);
    fprintf(output_file, "\n");
    fprintf(output_file, "Current PC = %i:\n", if_is.NPC - 4);
    fprintf(output_file, "\n");
    fprintf(output_file, "Pipeline Status:\n");
    fprintf(output_file, " * IF : %s\n", if_is.status);
    if (strcmp(is_id.status, "NOP") == 0) {
        fprintf(output_file, " * IS : %s\n", is_id.status);
    } else {
        fprintf(output_file, " * IS : <fetched %s>\n", is_id.status);
    }
    if ((strcmp(rf_ex.status, "**STALL**") == 0) &&
        ((strcmp(ex_df.status, "**STALL**") == 0)) &&
        ((strcmp(df_ds.status, "**STALL**") == 0)) &&
        (!(strcmp(ds_wb.status, "**STALL**") == 0)) &&
        (!(strcmp(id_rf.status, "RET") == 0))) {
            sprintf(id_rf.status, "**STALL**");
            fprintf(output_file, " * ID : %s\n", id_rf.status);
    } else {
        fprintf(output_file, " * ID : %s\n", id_rf.status);
    }
    fprintf(output_file, " * RF : %s\n", rf_ex.status);
    fprintf(output_file, " * EX : %s\n", ex_df.status);
    fprintf(output_file, " * DF : %s\n", df_ds.status);
    fprintf(output_file, " * DS : %s\n", ds_wb.status);
    fprintf(output_file, " * WB : %s\n", wb.status);
    fprintf(output_file, "\n");
    fprintf(output_file, "Stall Instruction: %s\n", CPUstate.stall_inst);
    fprintf(output_file, "\n");
    fprintf(output_file, "Forwarding:\n");
    fprintf(output_file, " Detected: %s\n", CPUstate.forwarding_inst);
    fprintf(output_file, " Forwarded:\n"); // FIX
    fprintf(output_file, " * EX/DF -> RF/EX : %s\n", CPUstate.ex_df_to_rf_ex);
    fprintf(output_file, " * DF/DS -> EX/DF : %s\n", CPUstate.df_ds_to_ex_df);
    fprintf(output_file, " * DF/DS -> RF/EX : %s\n", CPUstate.df_ds_to_rf_ex);
    fprintf(output_file, " * DS/WB -> EX/DF : %s\n", CPUstate.ds_wb_to_ex_df);
    fprintf(output_file, " * DS/WB -> RF/EX : %s\n", CPUstate.ds_wb_to_rf_ex);

    fprintf(output_file, "\n");
    fprintf(output_file, "Pipeline Registers:\n");
    fprintf(output_file, " * IF/IS.NPC: %i\n", if_is.NPC);
    if (strcmp(is_id.status, "NOP") == 0) {
        fprintf(output_file, " * IS/ID.IR: 0\n");
    } else {
        fprintf(output_file, " * IS/ID.IR: <%s>\n", is_id.status);
    }
    fprintf(output_file, " * RF/EX.A: %hi\n", rf_ex.rs1_data);
    fprintf(output_file, " * RF/EX.B: %hi\n", rf_ex.rs2_data);
    fprintf(output_file, " * EX/DF.ALUout: %hi\n", ex_df.alu_result);
    fprintf(output_file, " * EX/DF.B: %hi\n", ex_df.rs2_data);
    if (strcmp(ds_wb.instruction, "LW") == 0) {
        fprintf(output_file, " * DS/WB.ALUout-LMD: %hi\n", ds_wb.alu_result);
    } else if (strcmp(ds_wb.instruction, "SW") == 0) {
        fprintf(output_file, " * DS/WB.ALUout-LMD: %hi\n", ds_wb.mem_data);
    } else {
        fprintf(output_file, " * DS/WB.ALUout-LMD: 0\n");
    }
    fprintf(output_file, "\n");
    fprintf(output_file, "Integer Registers:\n");
    
    for (i = 0; i <= 7; i++) {
        for (j = 0; j <= 3; j++) {
            fprintf(output_file, "R%i       %hi ", (4*i)+j, register_file[(4*i)+j]);
        }
        fprintf(output_file, "\n");
    }

    fprintf(output_file, "\n");
    fprintf(output_file, "Data Memory:\n");
    for (i = 0; i < 10; i++) {
        fprintf(output_file, "%i: %hi\n", 600 + i*4, data_memory[i]);
    }

    fprintf(output_file, "\n");
    fprintf(output_file, "Total Stalls:\n");
    fprintf(output_file, " * Loads: %i\n", CPUstate.load_stalls);
    fprintf(output_file, " * Branches: %i\n", CPUstate.branch_stalls);
    fprintf(output_file, " * Other: %i\n", CPUstate.other_stalls);

    fprintf(output_file, "\n");
    fprintf(output_file, "Total Forwardings:\n");
    fprintf(output_file, " * EX/DF -> RF/EX : %i\n", CPUstate._ex_df_to_rf_ex);
    fprintf(output_file, " * DF/DS -> EX/DF : %i\n", CPUstate._df_ds_to_ex_df);
    fprintf(output_file, " * DF/DS -> RF/EX : %i\n", CPUstate._df_ds_to_rf_ex);
    fprintf(output_file, " * DS/WB -> EX/DF : %i\n", CPUstate._ds_wb_to_ex_df);
    fprintf(output_file, " * DS/WB -> RF/EX : %i\n", CPUstate._ds_wb_to_rf_ex);

    fprintf(output_file, "\n");
};

void print_summary(FILE* output_file) {
    int i;
    int j;

    fprintf(output_file, "**** Summary ************************************************\n");
    fprintf(output_file, "\n");
    fprintf(output_file, "BREAK PC = %i\n", CPUstate.break_pc);
    fprintf(output_file, "\n");
    fprintf(output_file, "Total Cycles Simulated = %i\n", CPUstate.cycle);
    fprintf(output_file, "\n");
    fprintf(output_file, "Integer Registers:\n");
    
    for (i = 0; i <= 8; i++) {
        for (j = 0; j <= 3; j++) {
            fprintf(output_file, "R%i       %hi ", i+j, register_file[i+j]);
        }
        fprintf(output_file, "\n");
    }

    fprintf(output_file, "\n");
    fprintf(output_file, "Data Memory:\n");
    for (i = 0; i < 10; i++) {
        fprintf(output_file, "%i: %hi\n", 600 + i*4, data_memory[i]);
    }

    fprintf(output_file, "\n");
    fprintf(output_file, "Total Stalls:\n");
    fprintf(output_file, " * Loads: %i\n", CPUstate.load_stalls);
    fprintf(output_file, " * Branches: %i\n", CPUstate.branch_stalls);
    fprintf(output_file, " * Other: %i\n", CPUstate.other_stalls);

    fprintf(output_file, "\n");
    fprintf(output_file, "Total Forwardings:\n");
    fprintf(output_file, " * EX/DF -> RF/EX : %i\n", CPUstate._ex_df_to_rf_ex);
    fprintf(output_file, " * DF/DS -> EX/DF : %i\n", CPUstate._df_ds_to_ex_df);
    fprintf(output_file, " * DF/DS -> RF/EX : %i\n", CPUstate._df_ds_to_rf_ex);
    fprintf(output_file, " * DS/WB -> EX/DF : %i\n", CPUstate._ds_wb_to_ex_df);
    fprintf(output_file, " * DS/WB -> RF/EX : %i\n", CPUstate._ds_wb_to_rf_ex);

    fprintf(output_file, "\n");
};

void simulate(FILE* input_file, FILE* output_file, int trace_start, int trace_end) {
initialize(input_file);
    while (!CPUstate.returned) {
        WB();
        DS();
        DF();
        EX();
        RF();
        ID();
        IS();
        IF();

        if (CPUstate.cycle >= trace_start && CPUstate.cycle <= trace_end) {
            trace_output(output_file);
        }

        handle_forward();

        CPUstate.cycle++;
    }
    print_summary(output_file);
}
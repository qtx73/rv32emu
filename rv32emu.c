#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

typedef enum {
    INSTR_LUI,
    INSTR_AUIPC,
    INSTR_JAL,
    INSTR_JALR,
    INSTR_BEQ,
    INSTR_BNE,
    INSTR_BLT,
    INSTR_BGE,
    INSTR_BLTU,
    INSTR_BGEU,
    INSTR_LB,
    INSTR_LH,
    INSTR_LW,
    INSTR_LBU,
    INSTR_LHU,
    INSTR_SB,
    INSTR_SH,
    INSTR_SW,
    INSTR_ADDI,
    INSTR_SLTI,
    INSTR_SLTIU,
    INSTR_XORI,
    INSTR_ORI,
    INSTR_ANDI,
    INSTR_SLLI,
    INSTR_SRLI,
    INSTR_SRAI,
    INSTR_ADD,
    INSTR_SUB,
    INSTR_SLL,
    INSTR_SLT,
    INSTR_SLTU,
    INSTR_XOR,
    INSTR_SRL,
    INSTR_SRA,
    INSTR_OR,
    INSTR_AND,
    INSTR_ECALL
} Instruction;

Instruction decoded_instr;

uint32_t pc; // Program counter
uint32_t reg[32]; // Register file
uint32_t mem[1 << 20]; // Memory

void execute_instr(uint32_t instr) {
    uint32_t opcode = instr & 0x7F;
    uint32_t rd = (instr >> 7) & 0x1F;
    uint32_t rs1 = (instr >> 15) & 0x1F;
    uint32_t rs2 = (instr >> 20) & 0x1F;
    uint32_t funct3 = (instr >> 12) & 0x7;
    uint32_t funct7 = (instr >> 25) & 0x7F;

    uint32_t imm_i = (int32_t) instr >> 20;
    uint32_t imm_s = ((int32_t) ((instr >> 25) << 5)) | ((instr >> 7) & 0x1F);
    uint32_t imm_b = ((int32_t) ((instr >> 31) << 12)) | ((instr >> 7) & 0x1) | ((instr >> 25) & 0x3E) | ((instr >> 8) & 0xF);
    uint32_t imm_u = instr & 0xFFFFF000;
    uint32_t imm_j = ((int32_t) ((instr >> 31) << 20)) | ((instr >> 21) & 0x3FF) | ((instr >> 20) & 0x1) | ((instr >> 12) & 0xFF);

    switch (opcode) {
        case 0x37 : // LUI (U-type)
            decoded_instr = INSTR_LUI;
            if (rd != 0)
                reg[rd] = imm_u;
            pc = pc + 4;
            break;
        case 0x17 : // AUIPC (U-type)
            decoded_instr = INSTR_AUIPC;
            if (rd != 0)
                reg[rd] = pc + imm_u;
            pc = pc + 4;
            break;
        case 0x6F : // JAL (J-type)
            decoded_instr = INSTR_JAL;
            if (rd != 0)
                reg[rd] = pc + 4;
            pc = pc + imm_j;
            break;
        case 0x67 : // JALR (I-type)
            decoded_instr = INSTR_JALR;
            uint32_t t = pc + 4;
            pc = (reg[rs1] + imm_i) & 0xFFFFFFFE;
            if (rd != 0)
                reg[rd] = t;
            break;
        case 0x63 : // Branch instructions
            switch (funct3) {
                case 0x0 : // BEQ
                    decoded_instr = INSTR_BEQ;
                    if (reg[rs1] == reg[rs2])
                        pc = pc + imm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x1 : // BNE
                    decoded_instr = INSTR_BNE;
                    if (reg[rs1] != reg[rs2])
                        pc = pc + imm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x4 : // BLT
                    decoded_instr = INSTR_BLT;
                    if ((int32_t) reg[rs1] < (int32_t) reg[rs2])
                        pc = pc + imm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x5 : // BGE
                    decoded_instr = INSTR_BGE;
                    if ((int32_t) reg[rs1] >= (int32_t) reg[rs2])
                        pc = pc + imm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x6 : // BLTU
                    decoded_instr = INSTR_BLTU;
                    if (reg[rs1] < reg[rs2])
                        pc = pc + imm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x7 : // BGEU
                    decoded_instr = INSTR_BGEU;
                    if (reg[rs1] >= reg[rs2])
                        pc = pc + imm_b;
                    else
                        pc = pc + 4;
                    break;
            }
            break;
        case 0x03 : // Load instructions
            switch (funct3) {
                uint32_t addr = reg[rs1] + imm_i;
                case 0x0 : // LB
                    decoded_instr = INSTR_LB;
                    if (rd != 0)
                        reg[rd] = mem[addr];
                    pc = pc + 4;
                    break;
                case 0x1 : // LH
                    decoded_instr = INSTR_LH;
                    if (rd != 0)
                        reg[rd] = mem[addr] | (mem[addr + 1] << 8);
                    pc = pc + 4;
                    break;
                case 0x2 : // LW
                    decoded_instr = INSTR_LW;
                    if (rd != 0)
                        reg[rd] = mem[addr] | (mem[addr + 1] << 8) | (mem[addr + 2] << 16) | (mem[addr + 3] << 24);
                    pc = pc + 4;
                    break;
                case 0x4 : // LBU
                    decoded_instr = INSTR_LBU;
                    if (rd != 0)
                        reg[rd] = mem[addr];
                    pc = pc + 4;
                    break;
                case 0x5 : // LHU
                    decoded_instr = INSTR_LHU;
                    if (rd != 0)
                        reg[rd] = mem[addr] | (mem[addr + 1] << 8);
                    pc = pc + 4;
                    break;
            }
            break;
        case 0x23 : // Store instructions
            switch (funct3) {
                uint32_t addr = reg[rs1] + imm_s;
                case 0x0 : // SB
                    decoded_instr = INSTR_SB;
                    mem[addr] = reg[rs2] & 0xFF;
                    pc = pc + 4;
                    break;
                case 0x1 : // SH
                    decoded_instr = INSTR_SH;
                    mem[addr] = reg[rs2] & 0xFF;
                    mem[addr + 1] = (reg[rs2] >> 8) & 0xFF;
                    pc = pc + 4;
                    break;
                case 0x2 : // SW
                    decoded_instr = INSTR_SW;
                    mem[addr] = reg[rs2] & 0xFF;
                    mem[addr + 1] = (reg[rs2] >> 8) & 0xFF;
                    mem[addr + 2] = (reg[rs2] >> 16) & 0xFF;
                    mem[addr + 3] = (reg[rs2] >> 24) & 0xFF;
                    pc = pc + 4;
                    break;
            }
            break;
        case 0x13 : // Immediate instructions
            switch (funct3) {
                case 0x0 : // ADDI
                    decoded_instr = INSTR_ADDI;
                    if (rd != 0)
                        reg[rd] = (int32_t) reg[rs1] + imm_i;
                    pc = pc + 4;
                    break;
                case 0x2 : // SLTI
                    decoded_instr = INSTR_SLTI;
                    if (rd != 0)
                        reg[rd] = ((int32_t) reg[rs1] < imm_i) ? 1 : 0;
                    pc = pc + 4;
                    break;
                case 0x3 : // SLTIU
                    decoded_instr = INSTR_SLTIU;
                    if (rd != 0)
                        reg[rd] = (reg[rs1] < imm_i) ? 1 : 0;
                    pc = pc + 4;
                    break;
                case 0x4 : // XORI
                    decoded_instr = INSTR_XORI;
                    if (rd != 0)
                        reg[rd] = reg[rs1] ^ imm_i;
                    pc = pc + 4;
                    break;
                case 0x6 : // ORI
                    decoded_instr = INSTR_ORI;
                    if (rd != 0)
                        reg[rd] = reg[rs1] | imm_i;
                    pc = pc + 4;
                    break;
                case 0x7 : // ANDI
                    decoded_instr = INSTR_ANDI;
                    if (rd != 0)
                        reg[rd] = reg[rs1] & imm_i;
                    pc = pc + 4;
                    break;
                case 0x1 : // SLLI
                    decoded_instr = INSTR_SLLI;
                    if (rd != 0)
                        reg[rd] = reg[rs1] << (imm_i & 0x1F);
                    pc = pc + 4;
                    break;
                case 0x5 : // SRLI or SRAI
                    if ((funct7 >> 5) == 0) {
                        decoded_instr = INSTR_SRLI;
                        if (rd != 0)
                            reg[rd] = reg[rs1] >> (imm_i & 0x1F);
                        pc = pc + 4;
                    } else {
                        decoded_instr = INSTR_SRAI;
                        if (rd != 0)
                            reg[rd] = ((int32_t) reg[rs1]) >> (imm_i & 0x1F);
                        pc = pc + 4;
                    }
                    break;
            }
            break;
        case 0x33 : // Register instructions
            switch (funct3) {
                case 0x0 : // ADD, SUB
                    if (funct7 == 0x00) {
                        decoded_instr = INSTR_ADD;
                        if (rd != 0)
                            reg[rd] = (int32_t) reg[rs1] + (int32_t) reg[rs2];
                        pc = pc + 4;
                    } else {
                        decoded_instr = INSTR_SUB;
                        if (rd != 0)
                            reg[rd] = (int32_t) reg[rs1] - (int32_t) reg[rs2];
                        pc = pc + 4;
                    }
                    break;
                case 0x1 : // SLL
                    decoded_instr = INSTR_SLL;
                    if (rd != 0)
                        reg[rd] = reg[rs1] << (reg[rs2] & 0x1F);
                    pc = pc + 4;
                    break;
                case 0x2 : // SLT
                    decoded_instr = INSTR_SLT;
                    if (rd != 0)
                        reg[rd] = ((int32_t) reg[rs1] < (int32_t) reg[rs2]) ? 1 : 0;
                    pc = pc + 4;
                    break;
                case 0x3 : // SLTU
                    decoded_instr = INSTR_SLTU;
                    if (rd != 0)
                        reg[rd] = (reg[rs1] < reg[rs2]) ? 1 : 0;
                    pc = pc + 4;
                    break;
                case 0x4 : // XOR
                    decoded_instr = INSTR_XOR;
                    if (rd != 0)
                        reg[rd] = reg[rs1] ^ reg[rs2];
                    pc = pc + 4;
                    break;
                case 0x5 : // SRL, SRA
                    if (funct7 == 0x00) {
                        decoded_instr = INSTR_SRL;
                        if (rd != 0)
                            reg[rd] = reg[rs1] >> (reg[rs2] & 0x1F);
                        pc = pc + 4;
                    } else {
                        decoded_instr = INSTR_SRA;
                        if (rd != 0)
                            reg[rd] = ((int32_t) reg[rs1]) >> (reg[rs2] & 0x1F);
                        pc = pc + 4;
                    }
                    break;
                case 0x6 : // OR
                    decoded_instr = INSTR_OR;
                    if (rd != 0)
                        reg[rd] = reg[rs1] | reg[rs2];
                    pc = pc + 4;
                    break;
                case 0x7 : // AND
                    decoded_instr = INSTR_AND;
                    if (rd != 0)
                        reg[rd] = reg[rs1] & reg[rs2];
                    pc = pc + 4;
                    break;
            }
            break;
        case 0x73 : // ECALL
            decoded_instr = INSTR_ECALL;
            exit(reg[3]);
            break;
        default :
            fprintf(stderr, "Error: Unknown instruction\n");
            exit(1);
    }
}

void print_decoded_instr() {
    switch (decoded_instr) {
        case INSTR_LUI :
            printf("LUI\n");
            break;
        case INSTR_AUIPC :
            printf("AUIPC\n");
            break;
        case INSTR_JAL :
            printf("JAL\n");
            break;
        case INSTR_JALR :
            printf("JALR\n");
            break;
        case INSTR_BEQ :
            printf("BEQ\n");
            break;
        case INSTR_BNE :
            printf("BNE\n");
            break;
        case INSTR_BLT :
            printf("BLT\n");
            break;
        case INSTR_BGE :
            printf("BGE\n");
            break;
        case INSTR_BLTU :
            printf("BLTU\n");
            break;
        case INSTR_BGEU :
            printf("BGEU\n");
            break;
        case INSTR_LB :
            printf("LB\n");
            break;
        case INSTR_LH :
            printf("LH\n");
            break;
        case INSTR_LW :
            printf("LW\n");
            break;
        case INSTR_LBU :
            printf("LBU\n");
            break;
        case INSTR_LHU :
            printf("LHU\n");
            break;
        case INSTR_SB :
            printf("SB\n");
            break;
        case INSTR_SH :
            printf("SH\n");
            break;
        case INSTR_SW :
            printf("SW\n");
            break;
        case INSTR_ADDI :
            printf("ADDI\n");
            break;
        case INSTR_SLTI :
            printf("SLTI\n");
            break;
        case INSTR_SLTIU :
            printf("SLTIU\n");
            break;
        case INSTR_XORI :
            printf("XORI\n");
            break;
        case INSTR_ORI :
            printf("ORI\n");
            break;
        case INSTR_ANDI :
            printf("ANDI\n");
            break;
        case INSTR_SLLI :
            printf("SLLI\n");
            break;
        case INSTR_SRLI :
            printf("SRLI\n");
            break;
        case INSTR_SRAI :
            printf("SRAI\n");
            break;
        case INSTR_ADD :
            printf("ADD\n");
            break;
        case INSTR_SUB :
            printf("SUB\n");
            break;
        case INSTR_SLL :
            printf("SLL\n");
            break;
        case INSTR_SLT :
            printf("SLT\n");
            break;
        case INSTR_SLTU :
            printf("SLTU\n");
            break;
        case INSTR_XOR :    
            printf("XOR\n");
            break;
        case INSTR_SRL :
            printf("SRL\n");
            break;
        case INSTR_SRA :
            printf("SRA\n");
            break;
        case INSTR_OR :
            printf("OR\n");
            break;
        case INSTR_AND :
            printf("AND\n");
            break;
        case INSTR_ECALL :
            printf("ECALL\n");
            break;
        default :
            fprintf(stderr, "Error: Unknown instruction\n");
            exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        return 1;
    }
    // size of file
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char buf[file_size];
    size_t size = fread(buf, 1, file_size, fp);

    // print the content of the file in 32-bit hexadecimal
    // with address
    for (int i = 0; i < size; i += 4) {
        printf("%08x: ", i);
        uint32_t instr = 0;
        for (int j = 0; j < 4; j++) {
            instr |= ((uint32_t)buf[i + j] << (j * 8)) & (0xFF << (j * 8));
        }
        printf("%08x\n", instr);
        execute_instr(instr);
        print_decoded_instr();
    }

    return 0;
}
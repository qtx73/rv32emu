#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define VLEN 128

uint32_t pc;           // Program counter
uint32_t xreg[32];     // Register file
uint8_t  mem[1 << 24]; // Memory

uint8_t  vreg[32][16]; // Vector Register file
uint32_t vl;           // Vector Length
uint32_t vtype;        // Vector Type Register

#define DEBUG
#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

void execute_vsetvli(uint32_t instr) {
    uint32_t rd = (instr >> 7) & 0x1F;
    uint32_t rs1 = (instr >> 15) & 0x1F;
    uint32_t imm_i = (instr >> 20) & 0x7FF;

    uint8_t vlmul = imm_i & 0x7;
    uint8_t vsew = (imm_i >> 3) & 0x7;
    uint8_t vta  = (imm_i >> 6) & 0x1;
    uint8_t vma  = (imm_i >> 7) & 0x1;
    
    // Check for invalid reserved bits or invalid vsew/vlmul
    if ((imm_i & 0xFFFFFFF0) != 0 || vsew > 0x3 || vlmul > 0x7) {
        vtype = 0x80000000; // Set vill bit (bit 31)
        vl = 0;
        if (rd != 0) xreg[rd] = 0;
        debug("vsetvli : xreg[0x%x] = 0x%x, vl = %d, vtype = 0x%x, vlmul = %d, vsew = %d, vta = %d, vma = %d\n",
        rd, rd != 0 ? xreg[rd] : 0, vl, vtype, vlmul, vsew, vta, vma);
        return;
    }

    // Compute SEW and LMUL
    uint32_t sew = 8 * (1 << vsew);
    int lmul_num, lmul_den;
    switch (vlmul) {
        case 0: lmul_num = 1; lmul_den = 1; break;
        case 1: lmul_num = 2; lmul_den = 1; break;
        case 2: lmul_num = 4; lmul_den = 1; break;
        case 3: lmul_num = 8; lmul_den = 1; break;
        case 5: lmul_num = 1; lmul_den = 8; break;
        case 6: lmul_num = 1; lmul_den = 4; break;
        case 7: lmul_num = 1; lmul_den = 2; break;
        default: vtype = 0x80000000; // Set vill bit (bit 31)
                 vl = 0;
                 if (rd != 0) xreg[rd] = 0;
                 debug("vsetvli : xreg[0x%x] = 0x%x, vl = %d, vtype = 0x%x, vlmul = %d, vsew = %d, vta = %d, vma = %d\n",
                 rd, rd != 0 ? xreg[rd] : 0, vl, vtype, vlmul, vsew, vta, vma);
                 return;
    }

    // Calucate VLMAX
    uint32_t vlmax = (VLEN / sew) * lmul_num / lmul_den;
    if (vlmax == 0) {
        vtype = 0x80000000; // Set vill bit (bit 31)
        vl = 0;
        if (rd != 0) xreg[rd] = 0;
        debug("vsetvli : xreg[0x%x] = 0x%x, vl = %d, vtype = 0x%x, vlmul = %d, vsew = %d, vta = %d, vma = %d\n",
        rd, rd != 0 ? xreg[rd] : 0, vl, vtype, vlmul, vsew, vta, vma);
        return;
    }

    // Compute AVL
    uint32_t avl;
    if (rs1 != 0) {
        avl = xreg[rs1];
    } else if (rd != 0) {
        avl = UINT32_MAX;
    } else {
        avl = vl;
    }

    // Set VL
    if (avl <= vlmax) {
        vl = avl;
    } else {
        vl = vlmax;
    }

    // Set VTYPE
    if (rd != 0) xreg[rd] = vl;
    vtype = (vma << 7) | (vta << 6) | (vsew << 3) | vlmul;   

    debug("vsetvli : xreg[0x%x] = 0x%x, vl = %d, vtype = 0x%x, vlmul = %d, vsew = %d, vta = %d, vma = %d\n", 
    rd, rd != 0 ? xreg[rd] : 0, vl, vtype, vlmul, vsew, vta, vma);
}

void execute_instr(uint32_t instr) {
    uint32_t opcode = instr & 0x7F;
    uint32_t rd = (instr >> 7) & 0x1F;
    uint32_t rs1 = (instr >> 15) & 0x1F;
    uint32_t rs2 = (instr >> 20) & 0x1F;
    uint32_t funct3 = (instr >> 12) & 0x7;
    uint32_t funct7 = (instr >> 25) & 0x7F;

    uint32_t imm_i = instr >> 20;
    uint32_t imm_s = ((instr >> 25) << 5) | ((instr >> 7) & 0x1F);
    uint32_t imm_b = ((instr >> 31) << 12) | (((instr >> 7) & 0x1) << 11) | (((instr >> 25) & 0x3F) << 5) | (((instr >> 8) & 0xF) << 1);
    uint32_t imm_u = instr & 0xFFFFF000;
    uint32_t imm_j = (((instr >> 31) & 0x01) << 20) | (((instr >> 21) & 0x3FF) << 1) | (((instr >> 20) & 0x1) << 11) | (((instr >> 12) & 0xFF) << 12);

    int32_t  simm_i = ((int32_t) imm_i << 20) >> 20;
    int32_t  simm_s = ((int32_t) imm_s << 20) >> 20;
    int32_t  simm_b = ((int32_t) imm_b << 19) >> 19;
    int32_t  simm_u = (int32_t) imm_u;
    int32_t  simm_j = ((int32_t) imm_j << 11) >> 11;

    switch (opcode) {
        case 0x37 : // LUI (U-type)
            if (rd != 0)
                xreg[rd] = simm_u;
            pc = pc + 4;
            debug("lui : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
            break;
        case 0x17 : // AUIPC (U-type)
            if (rd != 0)
                xreg[rd] = pc + simm_u;
            pc = pc + 4;
            debug("auipc : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
            break;
        case 0x6F : // JAL (J-type)
            if (rd != 0)
                xreg[rd] = pc + 4;
            pc = pc + simm_j;
            debug("jal : xreg[0x%x] = 0x%x, pc = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0, pc);
            break;
        case 0x67 :{// JALR (I-type)
            uint32_t t = pc + 4;
            pc = (xreg[rs1] + simm_i) & 0xFFFFFFFE;
            if (rd != 0)
                xreg[rd] = t;
            debug("jalr : xreg[0x%x] = 0x%x, pc = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0, pc);
            break;
        }
        case 0x63 : // Branch instructions
            switch (funct3) {
                case 0x0 : // BEQ
                    debug("beq : if(xreg[0x%x](0x%x) == xreg[0x%x](0x%x)) pc (0x%x) = 0x%x + 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b, pc, simm_b);
                    if (xreg[rs1] == xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x1 : // BNE
                    debug("bne : if(xreg[0x%x](0x%x) != xreg[0x%x](0x%x)) pc (0x%x) = 0x%x + 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b, pc, simm_b);
                    if (xreg[rs1] != xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x4 : // BLT
                    debug("blt : if(xreg[0x%x](0x%x) < xreg[0x%x](0x%x)) pc = 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b);
                    if ((int32_t) xreg[rs1] < (int32_t) xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x5 : // BGE
                    debug("bge : if(xreg[0x%x](0x%x) >= xreg[0x%x](0x%x)) pc = 0x%x\n", rs1, (int32_t)xreg[rs1], rs2, (int32_t)xreg[rs2], pc + simm_b);
                    if ((int32_t) xreg[rs1] >= (int32_t) xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x6 : // BLTU
                    debug("bltu : if(xreg[0x%x](0x%x) < xreg[0x%x](0x%x)) pc = 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b);
                    if (xreg[rs1] < xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    break;
                case 0x7 : // BGEU
                    debug("bgeu : if(xreg[0x%x](0x%x) >= xreg[0x%x](0x%x)) pc = 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b);
                    if (xreg[rs1] >= xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    break;
            }
            break;
        case 0x03 : {// Load instructions
            uint32_t addr = xreg[rs1] + simm_i;
            switch (funct3) {
                case 0x0 : {// LB
                    int32_t val = ((int32_t) mem[addr] << 24) >> 24;
                    if (rd != 0)
                        xreg[rd] = val;
                    pc = pc + 4;
                    debug("lb : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    break;
                }
                case 0x1 : {// LH
                    int32_t val = mem[addr] | (mem[addr + 1] << 8);
                    val = (val << 16) >> 16;
                    if (rd != 0)
                        xreg[rd] = val;
                    pc = pc + 4;
                    debug("lh : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    break;
                }
                case 0x2 : {// LW
                    int32_t val = mem[addr] | (mem[addr + 1] << 8) | (mem[addr + 2] << 16) | (mem[addr + 3] << 24);
                    if (rd != 0)
                        xreg[rd] = val;
                    pc = pc + 4;
                    debug("lw : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    break;
                }
                case 0x4 : // LBU
                    if (rd != 0)
                        xreg[rd] = mem[addr];
                    pc = pc + 4;
                    debug("lbu : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    break;
                case 0x5 : // LHU
                    if (rd != 0)
                        xreg[rd] = mem[addr] | (mem[addr + 1] << 8);
                    pc = pc + 4;
                    debug("lhu : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    break;
            }
            break;
        }
        case 0x23 : {// Store instructions
            uint32_t addr = xreg[rs1] + simm_s;
            switch (funct3) {
                case 0x0 : // SB
                    mem[addr] = xreg[rs2] & 0xFF;
                    pc = pc + 4;
                    debug("sb : mem[0x%x] = 0x%x\n", addr, xreg[rs2] & 0xFF);
                    break;
                case 0x1 : // SH
                    mem[addr] = xreg[rs2] & 0xFF;
                    mem[addr + 1] = (xreg[rs2] >> 8) & 0xFF;
                    pc = pc + 4;
                    debug("sh : mem[0x%x..0x%x] = 0x%x\n", addr, addr+1, xreg[rs2] & 0xFFFF);
                    break;
                case 0x2 : // SW
                    mem[addr] = xreg[rs2] & 0xFF;
                    mem[addr + 1] = (xreg[rs2] >> 8) & 0xFF;
                    mem[addr + 2] = (xreg[rs2] >> 16) & 0xFF;
                    mem[addr + 3] = (xreg[rs2] >> 24) & 0xFF;
                    pc = pc + 4;
                    debug("sw : mem[0x%x..0x%x] = 0x%x\n", addr, addr+3, xreg[rs2]);
                    break;
            }
            break;
        }
        case 0x13 : // Immediate instructions
            switch (funct3) {
                case 0x0 : // ADDI
                    debug("addi : xreg[0x%x](0x%x) = 0x%x + 0x%x\n",
                        rd, rd != 0 ? xreg[rd] + simm_i : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = (int32_t) xreg[rs1] + simm_i;
                    pc += 4;
                    break;
                case 0x2 : // SLTI
                    debug("slti : xreg[0x%x](0x%x) = (0x%x < 0x%x) ? 1 : 0\n",
                        rd, rd != 0 ? (xreg[rd] < simm_i) : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = ((int32_t) xreg[rs1] < simm_i) ? 1 : 0;
                    pc += 4;
                    break;
                case 0x3 : // SLTIU
                    debug("sltiu : xreg[0x%x](0x%x) = (%u < %u) ? 1 : 0\n",
                        rd, rd != 0 ? (xreg[rd] < simm_i) : 0,
                        xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = (xreg[rs1] < simm_i) ? 1 : 0;
                    pc += 4;
                    break;
                case 0x4 : // XORI
                    debug("xori : xreg[0x%x](0x%x) = 0x%x ^ 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] ^ simm_i;
                    pc += 4;
                    break;
                case 0x6 : // ORI
                    debug("ori : xreg[0x%x](0x%x) = 0x%x | 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] | simm_i;
                    pc += 4;
                    break;
                case 0x7 : // ANDI
                    debug("andi : xreg[0x%x](0x%x) = 0x%x & 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] & simm_i;
                    pc += 4;
                    break;
                case 0x1 : // SLLI
                    debug("slli : xreg[0x%x](0x%x) = 0x%x << 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (imm_i & 0x1F));
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] << (imm_i & 0x1F);
                    pc += 4;
                    break;
                case 0x5 : // SRLI or SRAI
                    if ((funct7 >> 5) == 0) {
                        debug("srli : xreg[0x%x](0x%x) = 0x%x >> 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (imm_i & 0x1F));
                        if (rd != 0)
                            xreg[rd] = xreg[rs1] >> (imm_i & 0x1F);
                        pc += 4;
                    } else {
                        debug("srai : xreg[0x%x](0x%x) = 0x%x >> 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (imm_i & 0x1F));
                        if (rd != 0)
                            xreg[rd] = ((int32_t) xreg[rs1]) >> (imm_i & 0x1F);
                        pc += 4;
                    }
                    break;
            }
            break;
        case 0x33 : // Register instructions
            switch (funct3) {
                case 0x0 : // ADD, SUB
                    if (funct7 == 0x00) {
                        if (rd != 0)
                            xreg[rd] = (int32_t) xreg[rs1] + (int32_t) xreg[rs2];
                        pc += 4;
                        debug("add : xreg[0x%x](0x%x) = 0x%x + 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    } else {
                        if (rd != 0)
                            xreg[rd] = (int32_t) xreg[rs1] - (int32_t) xreg[rs2];
                        pc += 4;
                        debug("sub : xreg[0x%x](0x%x) = 0x%x - 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    }
                    break;
                case 0x1 : // SLL
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] << (xreg[rs2] & 0x1F);
                    pc += 4;
                    debug("sll : xreg[0x%x](0x%x) = 0x%x << 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        xreg[rs1], (xreg[rs2] & 0x1F));
                    break;
                case 0x2 : // SLT
                    if (rd != 0)
                        xreg[rd] = ((int32_t) xreg[rs1] < (int32_t) xreg[rs2]) ? 1 : 0;
                    pc += 4;
                    debug("slt : xreg[0x%x](0x%x) = (0x%x < 0x%x) ? 1 : 0\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    break;
                case 0x3 : // SLTU
                    if (rd != 0)
                        xreg[rd] = (xreg[rs1] < xreg[rs2]) ? 1 : 0;
                    pc += 4;
                    debug("sltu : xreg[0x%x](0x%x) = (%u < %u) ? 1 : 0\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        xreg[rs1], xreg[rs2]);
                    break;
                case 0x4 : // XOR
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] ^ xreg[rs2];
                    pc += 4;
                    debug("xor : xreg[0x%x](0x%x) = 0x%x ^ 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    break;
                case 0x5 : // SRL, SRA
                    if (funct7 == 0x00) {
                        if (rd != 0)
                            xreg[rd] = xreg[rs1] >> (xreg[rs2] & 0x1F);
                        pc += 4;
                        debug("srl : xreg[0x%x](0x%x) = 0x%x >> 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (xreg[rs2] & 0x1F));
                    } else {
                        if (rd != 0)
                            xreg[rd] = ((int32_t) xreg[rs1]) >> (xreg[rs2] & 0x1F);
                        pc += 4;
                        debug("sra : xreg[0x%x](0x%x) = 0x%x >> 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (xreg[rs2] & 0x1F));
                    }
                    break;
                case 0x6 : // OR
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] | xreg[rs2];
                    pc += 4;
                    debug("or : xreg[0x%x](0x%x) = 0x%x | 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    break;
                case 0x7 : // AND
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] & xreg[rs2];
                    pc += 4;
                    debug("and : xreg[0x%x](0x%x) = 0x%x & 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    break;
            }
            break;
        case 0x73 : // ECALL
            if (instr == 0x73) {
                debug("ecall : exit(0x%x)\n", xreg[3]);
                exit(xreg[3]);
            }
            else {
                uint32_t csr_addr = (instr >> 20) & 0xFFF;
                uint32_t zimm = (instr >> 15) & 0x1F;
                switch (funct3) {
                    default:
                        pc = pc + 4;
                        debug("unknown system instruction : pc = 0x%x\n", pc);
                        break;
                    }
            }
            break;
        case 0x57 : // VSETVLI
            if (funct3 == 0x7) {
                pc = pc + 4;
                execute_vsetvli(instr);
            } else {
                pc = pc + 4;
                debug("unknown : pc = 0x%x\n", pc);
            }
            break;
        default :
            pc = pc + 4;
            debug("unknown : pc = 0x%x\n", pc);
            break;
            //fprintf(stderr, "Error: Unknown instruction\n");
            //exit(1);
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

    size_t size = fread(mem, 1, file_size, fp);

    int MAX = 8000;
    pc = 0;
    // print the content of the file in 32-bit hexadecimal
    // with address
    for (int i = 0; i < MAX; i += 4) {
        printf("%08x: ", pc);
        uint32_t instr = 0;
        for (int j = 0; j < 4; j++) {
            instr |= ((uint32_t)mem[pc + j] << (j * 8)) & (0xFF << (j * 8));
        }
        printf("%08x\n", instr);
        execute_instr(instr);
        printf("--------------------\n");
    }

    return -1;
}

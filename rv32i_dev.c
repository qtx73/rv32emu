#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "rv32.h"

extern uint32_t pc;      // Program counter
extern uint32_t xreg[32]; // Register file
extern uint8_t  mem[1 << 24]; // Memory

int decode_rv32i_instr(uint32_t instr) {
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
            return 1;
        case 0x17 : // AUIPC (U-type)
            if (rd != 0)
                xreg[rd] = pc + simm_u;
            pc = pc + 4;
            debug("auipc : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
            return 1;
        case 0x6F : // JAL (J-type)
            if (rd != 0)
                xreg[rd] = pc + 4;
            pc = pc + simm_j;
            debug("jal : xreg[0x%x] = 0x%x, pc = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0, pc);
            return 1;
        case 0x67 : {// JALR (I-type)
            uint32_t t = pc + 4;
            pc = (xreg[rs1] + simm_i) & 0xFFFFFFFE;
            if (rd != 0)
                xreg[rd] = t;
            debug("jalr : xreg[0x%x] = 0x%x, pc = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0, pc);
            return 1;
        }
        case 0x63 : // Branch instructions
            switch (funct3) {
                case 0x0 : // BEQ
                    debug("beq : if(xreg[0x%x](0x%x) == xreg[0x%x](0x%x)) pc (0x%x) = 0x%x + 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b, pc, simm_b);
                    if (xreg[rs1] == xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    return 1;
                case 0x1 : // BNE
                    debug("bne : if(xreg[0x%x](0x%x) != xreg[0x%x](0x%x)) pc (0x%x) = 0x%x + 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b, pc, simm_b);
                    if (xreg[rs1] != xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    return 1;
                case 0x4 : // BLT
                    debug("blt : if(xreg[0x%x](0x%x) < xreg[0x%x](0x%x)) pc = 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b);
                    if ((int32_t) xreg[rs1] < (int32_t) xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    return 1;
                case 0x5 : // BGE
                    debug("bge : if(xreg[0x%x](0x%x) >= xreg[0x%x](0x%x)) pc = 0x%x\n", rs1, (int32_t)xreg[rs1], rs2, (int32_t)xreg[rs2], pc + simm_b);
                    if ((int32_t) xreg[rs1] >= (int32_t) xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    return 1;
                case 0x6 : // BLTU
                    debug("bltu : if(xreg[0x%x](0x%x) < xreg[0x%x](0x%x)) pc = 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b);
                    if (xreg[rs1] < xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    return 1;
                case 0x7 : // BGEU
                    debug("bgeu : if(xreg[0x%x](0x%x) >= xreg[0x%x](0x%x)) pc = 0x%x\n", rs1, xreg[rs1], rs2, xreg[rs2], pc + simm_b);
                    if (xreg[rs1] >= xreg[rs2])
                        pc = pc + simm_b;
                    else
                        pc = pc + 4;
                    return 1;
            }
        case 0x03 : {// Load instructions
            uint32_t addr = xreg[rs1] + simm_i;
            switch (funct3) {
                case 0x0 : {// LB
                    int32_t val = ((int32_t) mem[addr] << 24) >> 24;
                    if (rd != 0)
                        xreg[rd] = val;
                    pc = pc + 4;
                    debug("lb : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    return 1;
                }
                case 0x1 : {// LH
                    int32_t val = mem[addr] | (mem[addr + 1] << 8);
                    val = (val << 16) >> 16;
                    if (rd != 0)
                        xreg[rd] = val;
                    pc = pc + 4;
                    debug("lh : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    return 1;
                }
                case 0x2 : {// LW
                    int32_t val = mem[addr] | (mem[addr + 1] << 8) | (mem[addr + 2] << 16) | (mem[addr + 3] << 24);
                    if (rd != 0)
                        xreg[rd] = val;
                    pc = pc + 4;
                    debug("lw : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    return 1;
                }
                case 0x4 : // LBU
                    if (rd != 0)
                        xreg[rd] = mem[addr];
                    pc = pc + 4;
                    debug("lbu : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    return 1;
                case 0x5 : // LHU
                    if (rd != 0)
                        xreg[rd] = mem[addr] | (mem[addr + 1] << 8);
                    pc = pc + 4;
                    debug("lhu : xreg[0x%x] = 0x%x\n", rd, rd != 0 ? xreg[rd] : 0);
                    return 1;
            }
        }
        case 0x23 : {// Store instructions
            uint32_t addr = xreg[rs1] + simm_s;
            switch (funct3) {
                case 0x0 : // SB
                    mem[addr] = xreg[rs2] & 0xFF;
                    pc = pc + 4;
                    debug("sb : mem[0x%x] = 0x%x\n", addr, xreg[rs2] & 0xFF);
                    return 1;
                case 0x1 : // SH
                    mem[addr] = xreg[rs2] & 0xFF;
                    mem[addr + 1] = (xreg[rs2] >> 8) & 0xFF;
                    pc = pc + 4;
                    debug("sh : mem[0x%x..0x%x] = 0x%x\n", addr, addr+1, xreg[rs2] & 0xFFFF);
                    return 1;
                case 0x2 : // SW
                    mem[addr] = xreg[rs2] & 0xFF;
                    mem[addr + 1] = (xreg[rs2] >> 8) & 0xFF;
                    mem[addr + 2] = (xreg[rs2] >> 16) & 0xFF;
                    mem[addr + 3] = (xreg[rs2] >> 24) & 0xFF;
                    pc = pc + 4;
                    debug("sw : mem[0x%x..0x%x] = 0x%x\n", addr, addr+3, xreg[rs2]);
                    return 1;
            }
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
                    return 1;
                case 0x2 : // SLTI
                    debug("slti : xreg[0x%x](0x%x) = (0x%x < 0x%x) ? 1 : 0\n",
                        rd, rd != 0 ? (xreg[rd] < simm_i) : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = ((int32_t) xreg[rs1] < simm_i) ? 1 : 0;
                    pc += 4;
                    return 1;
                case 0x3 : // SLTIU
                    debug("sltiu : xreg[0x%x](0x%x) = (%u < %u) ? 1 : 0\n",
                        rd, rd != 0 ? (xreg[rd] < simm_i) : 0,
                        xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = (xreg[rs1] < simm_i) ? 1 : 0;
                    pc += 4;
                    return 1;
                case 0x4 : // XORI
                    debug("xori : xreg[0x%x](0x%x) = 0x%x ^ 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] ^ simm_i;
                    pc += 4;
                    return 1;
                case 0x6 : // ORI
                    debug("ori : xreg[0x%x](0x%x) = 0x%x | 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] | simm_i;
                    pc += 4;
                    return 1;
                case 0x7 : // ANDI
                    debug("andi : xreg[0x%x](0x%x) = 0x%x & 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], simm_i);
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] & simm_i;
                    pc += 4;
                    return 1;
                case 0x1 : // SLLI
                    debug("slli : xreg[0x%x](0x%x) = 0x%x << 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (imm_i & 0x1F));
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] << (imm_i & 0x1F);
                    pc += 4;
                    return 1;
                case 0x5 : // SRLI or SRAI
                    if ((funct7 >> 5) == 0) {
                        debug("srli : xreg[0x%x](0x%x) = 0x%x >> 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (imm_i & 0x1F));
                        if (rd != 0)
                            xreg[rd] = xreg[rs1] >> (imm_i & 0x1F);
                        pc += 4;
                        return 1;
                    } else {
                        debug("srai : xreg[0x%x](0x%x) = 0x%x >> 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (imm_i & 0x1F));
                        if (rd != 0)
                            xreg[rd] = ((int32_t) xreg[rs1]) >> (imm_i & 0x1F);
                        pc += 4;
                        return 1;
                    }
            }
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
                        return 1;
                    } else if (funct7 == 0x20) {
                        if (rd != 0)
                            xreg[rd] = (int32_t) xreg[rs1] - (int32_t) xreg[rs2];
                        pc += 4;
                        debug("sub : xreg[0x%x](0x%x) = 0x%x - 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                        return 1;
                    }
                    return 0;
                case 0x1 : // SLL
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] << (xreg[rs2] & 0x1F);
                    pc += 4;
                    debug("sll : xreg[0x%x](0x%x) = 0x%x << 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        xreg[rs1], (xreg[rs2] & 0x1F));
                    return 1;
                case 0x2 : // SLT
                    if (rd != 0)
                        xreg[rd] = ((int32_t) xreg[rs1] < (int32_t) xreg[rs2]) ? 1 : 0;
                    pc += 4;
                    debug("slt : xreg[0x%x](0x%x) = (0x%x < 0x%x) ? 1 : 0\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    return 1;
                case 0x3 : // SLTU
                    if (rd != 0)
                        xreg[rd] = (xreg[rs1] < xreg[rs2]) ? 1 : 0;
                    pc += 4;
                    debug("sltu : xreg[0x%x](0x%x) = (%u < %u) ? 1 : 0\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        xreg[rs1], xreg[rs2]);
                    return 1;
                case 0x4 : // XOR
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] ^ xreg[rs2];
                    pc += 4;
                    debug("xor : xreg[0x%x](0x%x) = 0x%x ^ 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    return 1;
                case 0x5 : // SRL, SRA
                    if (funct7 == 0x00) {
                        if (rd != 0)
                            xreg[rd] = xreg[rs1] >> (xreg[rs2] & 0x1F);
                        pc += 4;
                        debug("srl : xreg[0x%x](0x%x) = 0x%x >> 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (xreg[rs2] & 0x1F));
                        return 1;
                    } else {
                        if (rd != 0)
                            xreg[rd] = ((int32_t) xreg[rs1]) >> (xreg[rs2] & 0x1F);
                        pc += 4;
                        debug("sra : xreg[0x%x](0x%x) = 0x%x >> 0x%x\n",
                            rd, rd != 0 ? xreg[rd] : 0,
                            (int32_t) xreg[rs1], (xreg[rs2] & 0x1F));
                        return 1;
                    }
                case 0x6 : // OR
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] | xreg[rs2];
                    pc += 4;
                    debug("or : xreg[0x%x](0x%x) = 0x%x | 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    return 1;
                case 0x7 : // AND
                    if (rd != 0)
                        xreg[rd] = xreg[rs1] & xreg[rs2];
                    pc += 4;
                    debug("and : xreg[0x%x](0x%x) = 0x%x & 0x%x\n",
                        rd, rd != 0 ? xreg[rd] : 0,
                        (int32_t) xreg[rs1], (int32_t) xreg[rs2]);
                    return 1;
            }
        case 0x73 : // ECALL
            if (instr == 0x73) {
                debug("ecall : exit(0x%x)\n", xreg[3]);
                exit(xreg[3]);
            } else {
                return 0;
            }
    }
    return 0;
}
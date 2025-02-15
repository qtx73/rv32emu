#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>  // For INT32_MIN

#include "rv32.h"

extern uint32_t pc;         // Program counter
extern uint32_t xreg[32];   // Register file
extern uint8_t  mem[1 << 24]; // Memory

/*
 * decode_rv32m_instr:
 *
 * Emulate an RV32M (M-extension) instruction.
 * Only instructions with opcode 0x33 and funct7 0x01 are processed.
 *
 * RV32M instructions:
 *   MUL    (funct3=0x0) : x[rd] = lower 32 bits of ( (int32_t)x[rs1] * (int32_t)x[rs2] )
 *   MULH   (funct3=0x1) : x[rd] = upper 32 bits of ( (int32_t)x[rs1] * (int32_t)x[rs2] )
 *   MULHSU (funct3=0x2) : x[rd] = upper 32 bits of ( (int32_t)x[rs1] * (uint32_t)x[rs2] )
 *   MULHU  (funct3=0x3) : x[rd] = upper 32 bits of ( (uint32_t)x[rs1] * (uint32_t)x[rs2] )
 *   DIV    (funct3=0x4) : x[rd] = (int32_t)x[rs1] / (int32_t)x[rs2]
 *                          (with dividend INT32_MIN and divisor -1 returning INT32_MIN;
 *                           division by 0 returns -1)
 *   DIVU   (funct3=0x5) : x[rd] = (uint32_t)x[rs1] / (uint32_t)x[rs2]
 *                          (division by 0 returns 0xFFFFFFFF)
 *   REM    (funct3=0x6) : x[rd] = (int32_t)x[rs1] % (int32_t)x[rs2]
 *                          (with dividend INT32_MIN and divisor -1 returning 0;
 *                           division by 0 returns x[rs1])
 *   REMU   (funct3=0x7) : x[rd] = (uint32_t)x[rs1] % (uint32_t)x[rs2]
 *                          (division by 0 returns x[rs1])
 */
int decode_rv32m_instr(uint32_t instr) {
    uint32_t opcode = instr & 0x7F;
    uint32_t rd     = (instr >> 7)  & 0x1F;
    uint32_t rs1    = (instr >> 15) & 0x1F;
    uint32_t rs2    = (instr >> 20) & 0x1F;
    uint32_t funct3 = (instr >> 12) & 0x7;
    uint32_t funct7 = (instr >> 25) & 0x7F;

    // Process only RV32M instructions (opcode 0x33 with funct7 0x01)
    if (opcode != 0x33 || funct7 != 0x01) {
        return 0; // Not an RV32M instruction.
    }

    switch (funct3) {
        case 0x0: { // MUL
            int64_t result = (int64_t)((int32_t)xreg[rs1]) * (int64_t)((int32_t)xreg[rs2]);
            if (rd != 0)
                xreg[rd] = (uint32_t)result;
            pc += 4;
            debug("mul : xreg[0x%x] = (0x%x * 0x%x) = 0x%x\n",
                  rd, xreg[rs1], xreg[rs2], rd ? xreg[rd] : 0);
            return 1;
        }
        case 0x1: { // MULH
            int64_t result = (int64_t)((int32_t)xreg[rs1]) * (int64_t)((int32_t)xreg[rs2]);
            if (rd != 0)
                xreg[rd] = (uint32_t)(((uint64_t)result) >> 32);
            pc += 4;
            debug("mulh : xreg[0x%x] = upper 32 bits of (0x%x * 0x%x) = 0x%x\n",
                  rd, xreg[rs1], xreg[rs2], rd ? xreg[rd] : 0);
            return 1;
        }
        case 0x2: { // MULHSU
            int64_t result = (int64_t)((int32_t)xreg[rs1]) * (uint64_t)xreg[rs2];
            if (rd != 0)
                xreg[rd] = (uint32_t)(((uint64_t)result) >> 32);
            pc += 4;
            debug("mulhsu : xreg[0x%x] = upper 32 bits of (0x%x * 0x%x) = 0x%x\n",
                  rd, xreg[rs1], xreg[rs2], rd ? xreg[rd] : 0);
            return 1;
        }
        case 0x3: { // MULHU
            uint64_t result = (uint64_t)xreg[rs1] * (uint64_t)xreg[rs2];
            if (rd != 0)
                xreg[rd] = (uint32_t)(result >> 32);
            pc += 4;
            debug("mulhu : xreg[0x%x] = upper 32 bits of (0x%x * 0x%x) = 0x%x\n",
                  rd, xreg[rs1], xreg[rs2], rd ? xreg[rd] : 0);
            return 1;
        }
        case 0x4: { // DIV
            int32_t dividend = (int32_t)xreg[rs1];
            int32_t divisor  = (int32_t)xreg[rs2];
            int32_t result;
            if (divisor == 0) {
                result = -1;
            } else if (dividend == INT32_MIN && divisor == -1) {
                result = INT32_MIN;
            } else {
                result = dividend / divisor;
            }
            if (rd != 0)
                xreg[rd] = (uint32_t)result;
            pc += 4;
            debug("div : xreg[0x%x] = (0x%x / 0x%x) = 0x%x\n",
                  rd, xreg[rs1], xreg[rs2], rd ? xreg[rd] : 0);
            return 1;
        }
        case 0x5: { // DIVU
            uint32_t dividend = xreg[rs1];
            uint32_t divisor  = xreg[rs2];
            uint32_t result;
            if (divisor == 0) {
                result = 0xFFFFFFFF;
            } else {
                result = dividend / divisor;
            }
            if (rd != 0)
                xreg[rd] = result;
            pc += 4;
            debug("divu : xreg[0x%x] = (0x%x / 0x%x) = 0x%x\n",
                  rd, xreg[rs1], xreg[rs2], rd ? xreg[rd] : 0);
            return 1;
        }
        case 0x6: { // REM
            int32_t dividend = (int32_t)xreg[rs1];
            int32_t divisor  = (int32_t)xreg[rs2];
            int32_t result;
            if (divisor == 0) {
                result = dividend;
            } else if (dividend == INT32_MIN && divisor == -1) {
                result = 0;
            } else {
                result = dividend % divisor;
            }
            if (rd != 0)
                xreg[rd] = (uint32_t)result;
            pc += 4;
            debug("rem : xreg[0x%x] = (0x%x %% 0x%x) = 0x%x\n",
                  rd, xreg[rs1], xreg[rs2], rd ? xreg[rd] : 0);
            return 1;
        }
        case 0x7: { // REMU
            uint32_t dividend = xreg[rs1];
            uint32_t divisor  = xreg[rs2];
            uint32_t result;
            if (divisor == 0) {
                result = dividend;
            } else {
                result = dividend % divisor;
            }
            if (rd != 0)
                xreg[rd] = result;
            pc += 4;
            debug("remu : xreg[0x%x] = (0x%x %% 0x%x) = 0x%x\n",
                  rd, xreg[rs1], xreg[rs2], rd ? xreg[rd] : 0);
            return 1;
        }
        default:
            return 0;
    }
}

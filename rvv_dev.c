#include <stdint.h>

#define VLEN 128

extern uint32_t pc;           // Program counter
extern uint32_t xreg[32];     // Register file
extern uint8_t  mem[1 << 24]; // Memory

uint8_t  vreg[32][16]; // Vector Register file
uint32_t vl;           // Vector Length
uint32_t vtype;        // Vector Type Register

uint8_t compute_avl(uint8_t rs1, uint8_t rd) {
    if (rs1 != 0) {
        return xreg[rs1] & 0x1F;
    } else if (rd != 0) {
        return UINT8_MAX;
    } else {
        return vl;
    }
}

void execute_vsetvl(uint8_t rd, uint8_t avl, uint32_t vtypei) {
    uint8_t vlmul = vtypei & 0x7;
    uint8_t vsew = (vtypei >> 3) & 0x7;
    uint8_t vta  = (vtypei >> 6) & 0x1;
    uint8_t vma  = (vtypei >> 7) & 0x1;

    // Check for invalid reserved bits or invalid vsew/vlmul
    if ((vtypei & 0xFFFFFF00) != 0 || vsew > 0x3 || vlmul > 0x7) {
        vtype = 0x80000000; // Set vill bit (bit 31)
        vl = 0;
        if (rd != 0) xreg[rd] = 0;
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
            return;
    }

    // Calculate VLMAX
    uint32_t vlmax = (VLEN * lmul_num) / (sew * lmul_den);
    if (vlmax == 0) {
        vtype = 0x80000000; // Set vill bit (bit 31)
        vl = 0;
        if (rd != 0) xreg[rd] = 0;
        return;
    }

    // Set VL
    if (avl <= vlmax) {
        vl = avl;
    } else {
        vl = vlmax;
    }
    if (rd != 0) xreg[rd] = vl;

    // Set VTYPE
    vtype = (vma << 7) | (vta << 6) | (vsew << 3) | vlmul;

    return;
}

void decode_rvv_instr(uint32_t instr) {
    uint32_t opcode = instr & 0x7F;
    switch (opcode) {
        case 0x57 : {
            uint8_t rd = (instr >> 7) & 0x1F;
            uint8_t rs1 = (instr >> 15) & 0x1F;
            uint8_t avl;
            uint8_t vtypei;
            if (((instr >> 25) &0x7) == 0x7) { // VSETVL
                if (((instr >> 31) & 1) == 0x0) { // vsetvli
                    pc = pc + 4;
                    avl = compute_avl(rs1, rd);
                    vtypei = (instr >> 20) & 0x3FF;
                }
                if (((instr >> 30) & 0x3) == 0x3) { // vsetivli
                    pc = pc + 4;
                    avl = (instr >> 15) & 0x1F;
                    vtypei = (instr >> 20) & 0x3FF;
                }
                if (((instr >> 30) & 0x3) == 0x2) { // vsetvl
                    pc = pc + 4;
                    avl = compute_avl(rs1, rd);
                    vtypei = xreg[(instr >> 20) & 0x1F];
                }
                execute_vsetvl(rd, avl, vtypei);
            }
        }    
    }
}
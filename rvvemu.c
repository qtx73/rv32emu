#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define VLEN 128

typedef struct rvvstate rvvstate;
struct rvvstate {
    uint32_t xreg[32];
    uint8_t  vreg[32][16];
    uint32_t vl;
    uint32_t vtype;
};


void execute_vsetvli(rvvstate *rvv, uint32_t instr) {
    uint32_t rd = (instr >> 7) & 0x1F;
    uint32_t rs1 = (instr >> 15) & 0x1F;
    uint32_t imm_i = (instr >> 20) & 0x7FF;

    uint8_t vlmul = imm_i & 0x7;
    uint8_t vsew = (imm_i >> 3) & 0x7;
    uint8_t vta  = (imm_i >> 6) & 0x1;
    uint8_t vma  = (imm_i >> 7) & 0x1;
    
    // Check for invalid reserved bits or invalid vsew/vlmul
    if ((imm_i & 0xFFFFFFF0) != 0 || vsew > 0x3 || vlmul > 0x7) {
        rvv->vtype = 0x80000000; // Set vill bit (bit 31)
        rvv->vl = 0;
        if (rd != 0) rvv->xreg[rd] = 0;
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
        default: rvv->vtype = 0x80000000; // Set vill bit (bit 31)
                 rvv->vl = 0;
                 if (rd != 0) rvv->xreg[rd] = 0;
                 return;
    }

    // Calucate VLMAX
    uint32_t vlmax = (VLEN / sew) * lmul_num / lmul_den;
    if (vlmax == 0) {
        rvv->vtype = 0x80000000; // Set vill bit (bit 31)
        rvv->vl = 0;
        if (rd != 0) rvv->xreg[rd] = 0;
        return;
    }

    // Compute AVL
    uint32_t avl;
    if (rs1 != 0) {
        avl = rvv->xreg[rs1];
    } else if (rd != 0) {
        avl = UINT32_MAX;
    } else {
        avl = rvv->vl;
    }

    // Set VL
    if (avl <= vlmax) {
        rvv->vl = avl;
    } else {
        rvv->vl = vlmax;
    }

    // Set VTYPE
    if (rd != 0) rvv->xreg[rd] = rvv->vl;
    rvv->vtype = (vma << 7) | (vta << 6) | (vsew << 3) | vlmul;    
}
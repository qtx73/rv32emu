#include <stdint.h>

#define VLEN 128

extern uint32_t pc;           // Program counter
extern uint32_t xreg[32];     // Register file
extern uint8_t  mem[1 << 24]; // Memory

uint8_t  vreg[32][VLEN/8]; // Vector Register file
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

void execute_vload(uint32_t instr) {
    uint8_t nf = (instr >> 29) & 0x7;
    uint8_t mew = (instr >> 28) & 0x1;
    uint8_t mop = (instr >> 26) & 0x3;
    uint8_t vm = (instr >> 25) & 0x1;
    uint8_t rs1 = (instr >> 15) & 0x1F;
    uint8_t width = (instr >> 12) & 0x7;
    uint8_t vd = (instr >> 7) & 0x1F;

    if (mew != 0) return ; // 128-bit load not supported

    uint8_t eew;
    switch (width) {
        case 0: eew = 1; break; // 8-bit
        case 1: eew = 2; break; // 16-bit
        case 2: eew = 4; break; // 32-bit
        default: return;
    }

    uint32_t base = xreg[rs1];

    uint8_t vmask[VLEN];
    for (uint32_t i = 0; i < vl; i++) {
        int byte_index = i / 8;
        int bit_index  = i % 8;
        vmask[i] = (vreg[0][byte_index] >> bit_index) & 0x1;
    }

    uint8_t NFIELDS = nf + 1;
    if (NFIELDS > 8) return;

    if (mop == 0) {
        uint8_t lumop = (instr >> 20) & 0x1F;
        if (lumop == 0x08) {  // Whole register load
            uint32_t evl = VLEN/eew;
            for (uint32_t i = 0; i < evl; i++) {
                for (uint32_t s = 0; s < NFIELDS; s++) {
                    uint32_t addr = base + i * NFIELDS * eew + s * eew;
                    for (uint32_t j = 0; j < eew; j++) {
                        if (vm == 1 || (vm == 0 && vmask[i] == 1))
                            vreg[vd + s][i * eew + j] = mem[addr + j];
                    }
                }
            }
            return;
        }
    }

    if (mop == 0x0 || mop == 0x2) {
        uint32_t stride;
        if (mop == 0) { // Unit-stride mode
            uint8_t lumop = (instr >> 20) & 0x1F;
            if (lumop == 0) { // Regular unit-stride
                ; // eew is set by width
            } else if (lumop == 0xB) { // unit-stride, mask load
                if (width != 0) return;
                if (nf != 0) return;
                eew = 1; // 8-bit
            }
            stride = eew;
        } else if (mop == 0x2) { // Strided mode.
            stride = (instr >> 20) & 0x1F; 
        } else {
            return;
        }

        for (uint32_t i = 0; i < vl; i++) {
            for (uint32_t s = 0; s < NFIELDS; s++) {
                uint32_t addr = base + i * stride * NFIELDS + s * stride;
                for (uint32_t j = 0; j < eew; j++) {
                    if (vm == 1 || (vm == 0 && vmask[i] == 1))
                        vreg[vd + s][i * eew + j] = mem[addr + j];
                }
            }
        }
        return;
    } else if (mop == 0x1 || mop == 0x3) {
        uint8_t index_reg = (instr >> 20) & 0x1F;
        for (uint32_t i = 0; i < vl; i++) {
            uint32_t offset = 0;
            uint8_t sew_bits = (vtype >> 3) & 0x7;
            switch (sew_bits) {
                case 0: { // 8-bit
                    offset = vreg[index_reg][i];
                    break;
                }
                case 1: { // 16-bit
                    offset = vreg[index_reg][i * 2] 
                            | (vreg[index_reg][i * 2 + 1] << 8);
                    break;
                }
                case 2: { // 32-bit
                    offset = vreg[index_reg][i * 4] 
                            | (vreg[index_reg][i * 4 + 1] << 8) 
                            | (vreg[index_reg][i * 4 + 2] << 16) 
                            | (vreg[index_reg][i * 4 + 3] << 24);
                    break;
                }
            }
            for (uint32_t s = 0; s < NFIELDS; s++) {
                uint32_t addr = base + offset + s * eew;
                for (uint32_t j = 0; j < eew; j++) {
                    if (vm == 1 || (vm == 0 && vmask[i] == 1))
                        vreg[vd + s][i * eew + j] = mem[addr + j];
                }
            }
        }
        return;
    }
}

void execute_vstore(uint32_t instr) {
    uint8_t nf = (instr >> 29) & 0x7;
    uint8_t mew = (instr >> 28) & 0x1;
    uint8_t mop = (instr >> 26) & 0x3;
    uint8_t vm = (instr >> 25) & 0x1;
    uint8_t rs1 = (instr >> 15) & 0x1F;
    uint8_t width = (instr >> 12) & 0x7;
    uint8_t vs3 = (instr >> 7) & 0x1F;

    if (mew != 0) return ;  // 128-bit store not supported

    uint8_t eew;
    switch (width) {
        case 0: eew = 1; break; // 8-bit
        case 1: eew = 2; break; // 16-bit
        case 2: eew = 4; break; // 32-bit
        default: return;
    }

    uint32_t base = xreg[rs1];

    uint8_t vmask[VLEN];
    for (uint32_t i = 0; i < vl; i++) {
        int byte_index = i / 8;
        int bit_index  = i % 8;
        vmask[i] = (vreg[0][byte_index] >> bit_index) & 0x1;
    }

    uint8_t NFIELDS = nf + 1;
    if (NFIELDS > 8) return;

    if (mop == 0x0) {
        uint8_t sumop = (instr >> 20) & 0x1F;
        if (sumop == 0x8) {  // Whole register store
            uint32_t evl = VLEN/eew;
            for (uint32_t i = 0; i < evl; i++) {
                for (uint32_t s = 0; s < NFIELDS; s++) {
                    uint32_t addr = base + i * NFIELDS * eew + s * eew;
                    for (uint32_t j = 0; j < eew; j++) {
                        if (vm == 1 || (vm == 0 && vmask[i] == 1))
                            mem[addr + j] = vreg[vs3 + s][i * eew + j];
                    }
                }
            }
            return;
        }
    }

    if (mop == 0x0 || mop == 0x2) {
        uint32_t stride;
        if (mop == 0) { // Unit-stride mode
            uint8_t sumop = (instr >> 20) & 0x1F;
            if (sumop == 0) { // Regular unit-stride
                ; // eew is set by width
            } else if (sumop == 0xB) { // unit-stride, mask load
                if (width != 0) return;
                if (nf != 0) return;
                eew = 1; // 8-bit
            }
            stride = eew;
        } else if (mop == 0x2) { // Strided mode.
            stride = (instr >> 20) & 0x1F; 
        } else {
            return;
        }

        for (uint32_t i = 0; i < vl; i++) {
            for (uint32_t s = 0; s < NFIELDS; s++) {
                uint32_t addr = base + i * stride * NFIELDS + s * stride;
                for (uint32_t j = 0; j < eew; j++) {
                    if (vm == 1 || (vm == 0 && vmask[i] == 1))
                        mem[addr + j] = vreg[vs3 + s][i * eew + j];
                }
            }
        }
    } else if (mop == 0x1 || mop == 0x3) {
        uint8_t index_reg = (instr >> 20) & 0x1F;
        for (uint32_t i = 0; i < vl; i++) {
            uint32_t offset = 0;
            uint8_t sew_bits = (vtype >> 3) & 0x7;
            switch (sew_bits) {
                case 0: { // 8-bit
                    offset = vreg[index_reg][i];
                    break;
                }
                case 1: { // 16-bit
                    offset = vreg[index_reg][i * 2] 
                            | (vreg[index_reg][i * 2 + 1] << 8);
                    break;
                }
                case 2: { // 32-bit
                    offset = vreg[index_reg][i * 4] 
                            | (vreg[index_reg][i * 4 + 1] << 8) 
                            | (vreg[index_reg][i * 4 + 2] << 16) 
                            | (vreg[index_reg][i * 4 + 3] << 24);
                    break;
                }
            }
            for (uint32_t s = 0; s < NFIELDS; s++) {
                uint32_t addr = base + offset + s * eew;
                for (uint32_t j = 0; j < eew; j++) {
                    if (vm == 1 || (vm == 0 && vmask[i] == 1))
                        mem[addr + j] = vreg[vs3 + s][i * eew + j];
                }
            }
        }
    }  
}

int32_t signed_extend(uint32_t val, uint8_t size) {
    if (val & (1 << (size - 1))) {
        return (int32_t)(val | (0xFFFFFFFF << size));
    } else {
        return (int32_t)val;
    }
}

void execute_varith(uint32_t instr) {
    // Extract fields
    uint8_t funct6 = (instr >> 26) & 0x3F;
    uint8_t funct3 = (instr >> 12) & 0x7;
    uint8_t vm = (instr >> 25) & 0x1;
    uint8_t vs2 = (instr >> 20) & 0x1F;

    // Build element-wise mask from v0
    uint8_t vmask[VLEN];
    for (uint32_t i = 0; i < vl; i++) {
        int byte_index = i / 8;
        int bit_index  = i % 8;
        vmask[i] = (vreg[0][byte_index] >> bit_index) & 0x1;
    }

    // Determine effective element width (in bytes)
    uint8_t vsew = (vtype >> 3) & 0x7;
    uint8_t eew = 1 << vsew;

    if (funct3 == 0x0 || funct3 == 0x3 || funct3 == 0x4) { // OPIVV, OPIVI, OPIVX
        uint8_t vd = (instr >> 7) & 0x1F; // Destination vector register vd

        for (uint32_t i = 0; i < vl; i++) {
            if (vm == 1 || (vm == 0 && vmask[i] == 1)) {
                uint32_t op1 = 0, op2 = 0, res = 0;
                int32_t op1s = 0, op2s = 0;

                // Load operands 2 from vector register vs2
                for (uint32_t j = 0; j < eew; j++) {
                    op2 |= (uint32_t)vreg[vs2][i * eew + j] << (j * 8);
                }
                op2s = signed_extend(op2, 8 * (1 << eew));

                // Select operand 1 based on funct3
                if (funct3 == 0x0) { // OPIVV : op1 from vector register vs1
                    uint8_t vs1 = (instr >> 15) & 0x1F;
                    for (uint32_t j = 0; j < eew; j++) {
                        op1 |= (uint32_t)vreg[vs1][i * eew + j] << (j * 8);
                        op1s = signed_extend(op1, 8 * (1 << eew));
                    }
                } else if (funct3 == 0x3) { // OPIVI : op1 from immediate[4:0]
                    op1 = (instr >> 15) & 0x1F;
                    op1s = signed_extend(op1, 5);
                } else if (funct3 == 0x4) { // OPIVX : op1 from x[rs1]
                    op1 = xreg[(instr >> 15) & 0x1F];
                    op1s = signed_extend(op1, 8 * (1 << eew));
                }

                // Perform operation based on funct6
                switch (funct6) {
                    case 0x00 : // add
                        res = op2s + op1s; 
                        break;
                    case 0x02 : // sub
                        res = op2s - op1s; 
                        break;
                    case 0x04 : // minu
                        res = (op2 < op1) ? op2 : op1; 
                        break;
                    case 0x05 : // min
                        res = (op2s < op1s) ? op2s : op1s; 
                        break;
                    case 0x06 : // maxu
                        res = (op2 > op1) ? op2 : op1; 
                        break;
                    case 0x07 : // max
                        res = (op2s > op1s) ? op2s : op1s; 
                        break;
                    case 0x09 : // and
                        res = op2 & op1; 
                        break;
                    case 0x0A : // or
                        res = op2 | op1; 
                        break;
                    case 0x0B : // xor
                        res = op2 ^ op1; 
                        break;
                    case 0x30 : // waddu
                        res = op2 + op1; 
                        break;
                    case 0x31 : // wadd
                        res = op2s + op1s; 
                        break;
                    case 0x32 : // wsubu
                        res = op2 - op1; 
                        break;
                    case 0x33 : // wsub
                        res = op2s - op1s; 
                        break;
                    case 0x34 : // waddu.w
                        res = op2 + op1; 
                        break;
                    case 0x35 : // wadd.w
                        res = op2s + op1s; 
                        break;
                    case 0x36 : // wsubu.w
                        res = op2 - op1; 
                        break;
                    case 0x37 : // wsub.w
                        res = op2s - op1s; 
                        break;
                    case 0x38 : // wmulu
                        res = op2 * op1; 
                        break;
                    case 0x3A : // wmulsu
                        res = op2s * op1;
                        break;
                    case 0x3B : // wmul
                        res = op2s * op1s; 
                        break;
                }

                // Write the result back to the destination vector register vd
                uint8_t write_back_eew = eew;
                if (funct6 >> 4 == 0x3) { // widening operation
                    if (eew == 1) {
                        write_back_eew = 2;
                    } else if (eew == 2) {
                        write_back_eew = 4;
                    } else {
                        write_back_eew = 8;
                    }
                } else if (funct6 >> 2 == 0xB) { // narrowing operation
                    if (eew == 8) {
                        write_back_eew = 4;
                    } else if (eew == 4) {
                        write_back_eew = 2;
                    } else {
                        write_back_eew = 1;
                    }
                }
                for (uint32_t j = 0; j < write_back_eew; j++) {
                    vreg[vd][i * write_back_eew + j] = (res >> (j * 8)) & 0xFF;
                }
            }
        }
    }
}

int decode_rvv_instr(uint32_t instr) {
    uint32_t opcode = instr & 0x7F;
    uint8_t funct3 = (instr >> 12) & 0x7;
    switch (opcode) {
        case 0x57 : 
            if (funct3 == 0x7) {
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
                return 1;
            } else {
                execute_varith(instr);
                return 1;
            }
        case 0x07: {
            pc = pc + 4;
            execute_vload(instr);
            return 1;
        }
        case 0x27: {
            pc = pc + 4;
            execute_vstore(instr);
            return 1;
        }
        default:
            return 0;
    }
}
#include <stdint.h>
#include <stdio.h>

#include "rv32.h"

#define VLEN 128

extern uint32_t pc;           // Program counter
extern uint32_t xreg[32];     // Register file
extern uint8_t  mem[1 << 24]; // Memory

uint8_t  vreg[32][VLEN/8]; // Vector Register file
uint32_t vl;           // Vector Length
uint32_t vtype;        // Vector Type Register

// Build vector mask from v0 register
void build_vmask(uint8_t vmask[VLEN]) {
    for (uint32_t i = 0; i < vl; i++) {
        int byte_index = i / 8;
        int bit_index  = i % 8;
        vmask[i] = (vreg[0][byte_index] >> bit_index) & 0x1;
    }
}

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
    // Decode instruction fields from the 32-bit instruction word
    uint8_t nf = (instr >> 29) & 0x7;       // Number of fields minus 1
    uint8_t mew = (instr >> 28) & 0x1;      // Memory element width
    uint8_t mop = (instr >> 26) & 0x3;      // Memory addressing mode
    uint8_t vm = (instr >> 25) & 0x1;       // Vector mask flag (1=unmasked, 0=masked)
    uint8_t rs1 = (instr >> 15) & 0x1F;     // Base address register (x-register)
    uint8_t width = (instr >> 12) & 0x7;    // Width encoding field
    uint8_t vd = (instr >> 7) & 0x1F;       // Destination vector register

    if (mew != 0) return; // 128-bit load not supported

    // Convert width encoding to effective element width (EEW) in bytes
    uint8_t eew;
    switch (width) {
        case 0: eew = 1; break; // 8-bit
        case 1: eew = 2; break; // 16-bit
        case 2: eew = 4; break; // 32-bit
        default: return;        // Unsupported width
    }

    // Calculate base address for memory operations
    uint32_t base = xreg[rs1];

    // Get mask bits from v0 register if masked operation (vm=0)
    uint8_t vmask[VLEN];
    build_vmask(vmask);

    // Calculate total number of fields to load
    uint8_t NFIELDS = nf + 1;
    if (NFIELDS > 8) return;    // Spec limits to maximum 8 fields

    // --- Handle whole register load mode ---
    if (mop == 0) {
        uint8_t lumop = (instr >> 20) & 0x1F;
        if (lumop == 0x08) {  // Whole register load unit-stride
            uint32_t evl = VLEN/eew;  // Elements per register
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

    // --- Handle unit-stride and strided modes ---
    if (mop == 0x0 || mop == 0x2) {
        uint32_t stride;
        if (mop == 0) {  // Unit-stride mode
            uint8_t lumop = (instr >> 20) & 0x1F;
            if (lumop == 0) {  // Regular unit-stride
                ; // eew already set by width
            } else if (lumop == 0xB) {  // Load mask bits (unit-stride)
                if (width != 0) return;  // Must be byte width
                if (nf != 0) return;     // Must be single-field
                eew = 1;  // Force 8-bit elements
            }
            stride = eew;  // In unit-stride, stride equals EEW
        } else if (mop == 0x2) {  // Strided mode
            stride = (instr >> 20) & 0x1F;  // Explicit stride value
        } else {
            return;  // Should be unreachable
        }

        // Load data from memory to vector registers
        for (uint32_t i = 0; i < vl; i++) {  // Loop through elements up to vector length
            for (uint32_t s = 0; s < NFIELDS; s++) {  // Loop through fields
                uint32_t addr = base + i * stride * NFIELDS + s * stride;
                for (uint32_t j = 0; j < eew; j++) {  // Loop through bytes in element
                    if (vm == 1 || (vm == 0 && vmask[i] == 1))
                        vreg[vd + s][i * eew + j] = mem[addr + j];
                }
            }
        }
        return;
    } 
    // --- Handle indexed modes ---
    else if (mop == 0x1 || mop == 0x3) {  // Indexed (unordered or ordered)
        uint8_t index_reg = (instr >> 20) & 0x1F;  // Register containing index values
        for (uint32_t i = 0; i < vl; i++) {
            // Extract offset from index register based on SEW
            uint32_t offset = 0;
            uint8_t sew_bits = (vtype >> 3) & 0x7;
            switch (sew_bits) {
                case 0: {  // 8-bit SEW
                    offset = vreg[index_reg][i];
                    break;
                }
                case 1: {  // 16-bit SEW
                    offset = vreg[index_reg][i * 2] 
                            | (vreg[index_reg][i * 2 + 1] << 8);
                    break;
                }
                case 2: {  // 32-bit SEW
                    offset = vreg[index_reg][i * 4] 
                            | (vreg[index_reg][i * 4 + 1] << 8) 
                            | (vreg[index_reg][i * 4 + 2] << 16) 
                            | (vreg[index_reg][i * 4 + 3] << 24);
                    break;
                }
            }
            // Load each field using calculated offset
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
    // Decode instruction fields
    uint8_t nf = (instr >> 29) & 0x7;       // Number of fields minus 1
    uint8_t mew = (instr >> 28) & 0x1;      // Memory element width
    uint8_t mop = (instr >> 26) & 0x3;      // Memory addressing mode
    uint8_t vm = (instr >> 25) & 0x1;       // Vector mask flag (1=unmasked, 0=masked)
    uint8_t rs1 = (instr >> 15) & 0x1F;     // Base address register (x-register)
    uint8_t width = (instr >> 12) & 0x7;    // Width encoding field
    uint8_t vs3 = (instr >> 7) & 0x1F;      // Source vector register

    if (mew != 0) return;  // 128-bit store not supported

    // Convert width encoding to effective element width (EEW) in bytes
    uint8_t eew;
    switch (width) {
        case 0: eew = 1; break; // 8-bit
        case 1: eew = 2; break; // 16-bit
        case 2: eew = 4; break; // 32-bit
        default: return;        // Unsupported width
    }

    // Calculate base address for memory operations
    uint32_t base = xreg[rs1];

    // Get mask bits from v0 register if masked operation (vm=0)
    uint8_t vmask[VLEN];
    build_vmask(vmask);

    // Calculate total number of fields to store
    uint8_t NFIELDS = nf + 1;
    if (NFIELDS > 8) return;    // Spec limits to maximum 8 fields

    // --- Handle whole register store mode ---
    if (mop == 0x0) {
        uint8_t sumop = (instr >> 20) & 0x1F;
        if (sumop == 0x8) {  // Whole register store
            uint32_t evl = VLEN/eew;  // Elements per register
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

    // --- Handle unit-stride and strided modes ---
    if (mop == 0x0 || mop == 0x2) {
        uint32_t stride;
        if (mop == 0) {  // Unit-stride mode
            uint8_t sumop = (instr >> 20) & 0x1F;
            if (sumop == 0) {  // Regular unit-stride
                ; // eew already set by width
            } else if (sumop == 0xB) {  // Store mask bits (unit-stride)
                if (width != 0) return;  // Must be byte width
                if (nf != 0) return;     // Must be single-field
                eew = 1;  // Force 8-bit elements
            }
            stride = eew;  // In unit-stride, stride equals EEW
        } else if (mop == 0x2) {  // Strided mode
            stride = (instr >> 20) & 0x1F;  // Explicit stride value
        } else {
            return;  // Should be unreachable
        }

        // Store data from vector registers to memory
        for (uint32_t i = 0; i < vl; i++) {  // Loop through elements up to vector length
            for (uint32_t s = 0; s < NFIELDS; s++) {  // Loop through fields
                uint32_t addr = base + i * stride * NFIELDS + s * stride;
                for (uint32_t j = 0; j < eew; j++) {  // Loop through bytes in element
                    if (vm == 1 || (vm == 0 && vmask[i] == 1))
                        mem[addr + j] = vreg[vs3 + s][i * eew + j];
                }
            }
        }
    } 
    // --- Handle indexed modes ---
    else if (mop == 0x1 || mop == 0x3) {  // Indexed (unordered or ordered)
        uint8_t index_reg = (instr >> 20) & 0x1F;  // Register containing index values
        for (uint32_t i = 0; i < vl; i++) {
            // Extract offset from index register based on SEW
            uint32_t offset = 0;
            uint8_t sew_bits = (vtype >> 3) & 0x7;
            switch (sew_bits) {
                case 0: {  // 8-bit SEW
                    offset = vreg[index_reg][i];
                    break;
                }
                case 1: {  // 16-bit SEW
                    offset = vreg[index_reg][i * 2] 
                            | (vreg[index_reg][i * 2 + 1] << 8);
                    break;
                }
                case 2: {  // 32-bit SEW
                    offset = vreg[index_reg][i * 4] 
                            | (vreg[index_reg][i * 4 + 1] << 8) 
                            | (vreg[index_reg][i * 4 + 2] << 16) 
                            | (vreg[index_reg][i * 4 + 3] << 24);
                    break;
                }
            }
            // Store each field using calculated offset
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
    // === Extract instruction fields ===
    uint8_t funct6 = (instr >> 26) & 0x3F;  // Operation type
    uint8_t funct3 = (instr >> 12) & 0x7;   // Instruction format
    uint8_t vm     = (instr >> 25) & 0x1;   // Masking mode
    uint8_t vs2    = (instr >> 20) & 0x1F;  // Source register 2
    uint8_t vd     = (instr >> 7) & 0x1F;   // Destination register
    
    // Build mask information from v0 register
    uint8_t vmask[VLEN];
    build_vmask(vmask);

    // Determine effective element width (in bytes)
    uint8_t vsew = (vtype >> 3) & 0x7;
    uint8_t eew = 1 << vsew;

    // Check if instruction is supported
    // OPIVV(0x0), OPIVI(0x3), OPIVX(0x4), OPMVV(0x2), OPMVX(0x6)
    if (funct3 == 0x0 || funct3 == 0x3 || funct3 == 0x4 || funct3 == 0x2 || funct3 == 0x6) { 
        // Process each vector element
        for (uint32_t i = 0; i < vl; i++) {
            // Check mask condition: if vm=1 execute unconditionally, if vm=0 follow mask value
            if (vm == 1 || (vm == 0 && vmask[i] == 1)) {
                // === Load operands ===
                uint32_t op1 = 0, op2 = 0, res = 0;
                int32_t op1s = 0, op2s = 0, ress = 0;
                uint32_t vd_val = 0;
                int32_t vd_vals = 0;
                
                // Load operand 2 from vs2 register
                for (uint32_t j = 0; j < eew; j++) {
                    op2 |= (uint32_t)vreg[vs2][i * eew + j] << (j * 8);
                }
                op2s = signed_extend(op2, 8 * eew);

                // Select operand 1 based on instruction format
                if (funct3 == 0x0 || funct3 == 0x2) { 
                    // OPIVV or OPMVV: Get operand 1 from vector register vs1
                    uint8_t vs1 = (instr >> 15) & 0x1F;
                    for (uint32_t j = 0; j < eew; j++) {
                        op1 |= (uint32_t)vreg[vs1][i * eew + j] << (j * 8);
                    }
                    op1s = signed_extend(op1, 8 * eew);
                } 
                else if (funct3 == 0x3) { 
                    // OPIVI: Get operand 1 from immediate value
                    op1 = (instr >> 15) & 0x1F;
                    op1s = signed_extend(op1, 5);
                } 
                else if (funct3 == 0x4 || funct3 == 0x6) { 
                    // OPIVX or OPMVX: Get operand 1 from scalar register
                    op1 = xreg[(instr >> 15) & 0x1F];
                    op1s = signed_extend(op1, 8 * eew);
                }

                // Load current vd value for fused operations
                if ((funct3 == 0x2 || funct3 == 0x6) && funct6 >= 0x20 && funct6 <= 0x23) {
                    for (uint32_t j = 0; j < eew; j++) {
                        vd_val |= (uint32_t)vreg[vd][i * eew + j] << (j * 8);
                    }
                    vd_vals = signed_extend(vd_val, 8 * eew);
                }
                
                // === Execute operation based on instruction format and opcode ===
                if (funct3 == 0x0 || funct3 == 0x3 || funct3 == 0x4) {
                    // Integer vector instructions (OPIVV, OPIVI, OPIVX)
                    switch (funct6) {
                        // Arithmetic operations
                        case 0x00: res = op2s + op1s; break;           // vadd
                        case 0x02: res = op2s - op1s; break;           // vsub
                        case 0x03: res = op1s - op2s; break;           // vrsub
                        
                        // Min/Max operations
                        case 0x04: res = (op2 < op1) ? op2 : op1; break;   // vminu
                        case 0x05: res = (op2s < op1s) ? op2s : op1s; break; // vmin
                        case 0x06: res = (op2 > op1) ? op2 : op1; break;   // vmaxu
                        case 0x07: res = (op2s > op1s) ? op2s : op1s; break; // vmax
                        
                        // Logical operations
                        case 0x09: res = op2 & op1; break;             // vand
                        case 0x0A: res = op2 | op1; break;             // vor
                        case 0x0B: res = op2 ^ op1; break;             // vxor
                        
                        // Comparison operations
                        case 0x10: res = (op2 == op1); break;          // vmseq
                        case 0x11: res = (op2 != op1); break;          // vmsne
                        case 0x12: res = (op2 < op1); break;           // vmsltu
                        case 0x13: res = (op2s < op1s); break;         // vmslt
                        case 0x14: res = (op2 <= op1); break;          // vmsleu
                        case 0x15: res = (op2s <= op1s); break;        // vmsle
                        case 0x16: res = (op2 > op1); break;           // vmsgtu
                        case 0x17: res = (op2s > op1s); break;         // vmsgt
                        
                        // Shift operations
                        case 0x25: res = op2 << op1; break;            // vsll
                        case 0x26: res = op2 >> op1; break;            // vsrl
                        case 0x27: res = op2s >> op1; break;           // vsra
                        case 0x2C: res = op2 >> op1; break;            // vnsrl
                        case 0x2D: res = op2s >> op1; break;           // vnsra
                        
                        // Widening operations
                        case 0x30: res = op2 + op1; break;             // vwaddu
                        case 0x31: res = op2s + op1s; break;           // vwadd
                        case 0x32: res = op2 - op1; break;             // vwsubu
                        case 0x33: res = op2s - op1s; break;           // vwsub
                        case 0x34: res = op2 + op1; break;             // vwaddu.w
                        case 0x35: res = op2s + op1s; break;           // vwadd.w
                        case 0x36: res = op2 - op1; break;             // vwsubu.w
                        case 0x37: res = op2s - op1s; break;           // vwsub.w
                    }
                } 
                else if (funct3 == 0x2 || funct3 == 0x6) {
                    // Multiplication/Division vector instructions (OPMVV, OPMVX)
                    switch (funct6) {
                        // Multiplication operations
                        case 0x08:
                            // vmul: Standard multiplication
                            res = op2s * op1s;
                            break;
                            
                        case 0x09:
                            // vmulh: Signed multiplication (high bits)
                            if (eew == 1) { // 8-bit
                                ress = ((int16_t)op2s * (int16_t)op1s) >> 8;
                            } else if (eew == 2) { // 16-bit
                                ress = ((int32_t)op2s * (int32_t)op1s) >> 16;
                            } else { // 32-bit (approximate calculation)
                                if ((op2s >> 16) == 0 && (op1s >> 16) == 0) {
                                    ress = 0; // If both values are small, high bits are 0
                                } else {
                                    ress = ((op2s >> 16) * op1s + (op1s >> 16) * (op2s & 0xFFFF));
                                }
                            }
                            res = (uint32_t)ress;
                            break;
                            
                        case 0x0A:
                            // vmulhu: Unsigned multiplication (high bits)
                            if (eew == 1) { // 8-bit
                                res = ((uint16_t)op2 * (uint16_t)op1) >> 8;
                            } else if (eew == 2) { // 16-bit
                                res = ((uint32_t)op2 * (uint32_t)op1) >> 16;
                            } else { // 32-bit (approximate calculation)
                                if ((op2 >> 16) == 0 && (op1 >> 16) == 0) {
                                    res = 0; // If both values are small, high bits are 0
                                } else {
                                    res = ((op2 >> 16) * op1 + (op1 >> 16) * (op2 & 0xFFFF));
                                }
                            }
                            break;
                            
                        case 0x0B:
                            // vmulhsu: Signed×Unsigned multiplication (high bits)
                            if (eew == 1) { // 8-bit
                                ress = ((int16_t)op2s * (uint16_t)op1) >> 8;
                            } else if (eew == 2) { // 16-bit
                                ress = ((int32_t)op2s * (uint32_t)op1) >> 16;
                            } else { // 32-bit (approximate calculation)
                                if ((op2s >> 16) == 0 && (op1 >> 16) == 0) {
                                    ress = 0; // If both values are small, high bits are 0
                                } else {
                                    ress = ((op2s >> 16) * op1 + (op1 >> 16) * (op2s & 0xFFFF));
                                }
                            }
                            res = (uint32_t)ress;
                            break;
                            
                        // Division operations
                        case 0x0C:
                            // vdiv: Signed division
                            if (op1s == 0) {
                                res = 0xFFFFFFFF; // Division by zero: set all bits to 1
                            } else {
                                ress = op2s / op1s;
                                res = (uint32_t)ress;
                            }
                            break;
                            
                        case 0x0D:
                            // vdivu: Unsigned division
                            if (op1 == 0) {
                                res = 0xFFFFFFFF; // Division by zero: set all bits to 1
                            } else {
                                res = op2 / op1;
                            }
                            break;
                            
                        case 0x0E:
                            // vrem: Signed remainder
                            if (op1s == 0) {
                                res = (uint32_t)op2s; // Division by zero: return dividend
                            } else {
                                ress = op2s % op1s;
                                res = (uint32_t)ress;
                            }
                            break;
                            
                        case 0x0F:
                            // vremu: Unsigned remainder
                            if (op1 == 0) {
                                res = op2; // Division by zero: return dividend
                            } else {
                                res = op2 % op1;
                            }
                            break;
                            
                        // Fused multiply operations
                        case 0x20:
                            // vmacc: Multiply-accumulate (vd = vd + (vs1 * vs2))
                            res = vd_val + (op1s * op2s);
                            break;
                            
                        case 0x21:
                            // vnmsac: Negate multiply-accumulate (vd = vd - (vs1 * vs2))
                            res = vd_val - (op1s * op2s);
                            break;
                            
                        case 0x22:
                            // vmadd: Multiply-add (vd = (vd * vs1) + vs2)
                            res = (vd_vals * op1s) + op2s;
                            break;
                            
                        case 0x23:
                            // vnmsub: Negate multiply-subtract (vd = -(vd * vs1) + vs2)
                            res = -(vd_vals * op1s) + op2s;
                            break;
                    }
                }

                // === Write back results ===
                // Determine write-back width based on operation
                uint8_t write_back_eew = eew;
                
                // For widening operations
                if (funct6 >> 4 == 0x3) {
                    if (eew == 1) {
                        write_back_eew = 2;       // 8bit -> 16bit
                    } else if (eew == 2) {
                        write_back_eew = 4;       // 16bit -> 32bit
                    } else {
                        write_back_eew = 8;       // 32bit -> 64bit
                    }
                } 
                // For narrowing operations
                else if (funct6 >> 2 == 0xB) {
                    if (eew == 8) {
                        write_back_eew = 4;       // 64bit -> 32bit
                    } else if (eew == 4) {
                        write_back_eew = 2;       // 32bit -> 16bit
                    } else {
                        write_back_eew = 1;       // 16bit -> 8bit
                    }
                } 
                // For mask operations
                else if (funct6 >> 3 == 0x3) {
                    write_back_eew = 1;           // Always 1-bit (mask value)
                }
                
                // Write result back to vector register
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
                if (((instr >> 12) &0x7) == 0x7) { // VSETVL
                    if (((instr >> 31) & 1) == 0x0) { // vsetvli
                        pc = pc + 4;
                        avl = compute_avl(rs1, rd);
                        vtypei = (instr >> 20) & 0x3FF;
                        execute_vsetvl(rd, avl, vtypei);
                        debug("vsetvli : vl=%d, vtype=%d\n", vl, vtype);
                        return 1;
                    }
                    if (((instr >> 30) & 0x3) == 0x3) { // vsetivli
                        pc = pc + 4;
                        avl = (instr >> 15) & 0x1F;
                        vtypei = (instr >> 20) & 0x3FF;
                        execute_vsetvl(rd, avl, vtypei);
                        debug("vsetivli : vl=%d, vtype=%d\n", vl, vtype);
                        return 1;
                    }
                    if (((instr >> 30) & 0x3) == 0x2) { // vsetvl
                        pc = pc + 4;
                        avl = compute_avl(rs1, rd);
                        vtypei = xreg[(instr >> 20) & 0x1F];
                        execute_vsetvl(rd, avl, vtypei);
                        debug("vsetvl : vl=%d, vtype=%d\n", vl, vtype);
                        return 1;
                    }
                    return 0;
                }
                return 0;
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

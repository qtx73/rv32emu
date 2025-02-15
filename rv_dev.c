#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "rv32.h"

uint32_t pc;       // Program counter
uint32_t xreg[32]; // Register file
uint8_t mem[1 << 18]; // Memory (256KB)

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

    // Get file size
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    //Load program into memory
    size_t max_mem_size = sizeof(mem);
    size_t read_size = file_size > max_mem_size ? max_mem_size : file_size;
    if (read_size != file_size) {
        fprintf(stderr, "Warning: File %s is too large, only %zu bytes will be loaded\n", argv[1], max_mem_size);
    }
    size_t bytes_load = fread(mem, 1, read_size, fp);
    if (bytes_load != read_size) {
        fprintf(stderr, "Error: fread failed to read file %s\n", argv[1]);
        return 1;
    }

    int max_cycle = 80;
    pc = 0;
    uint32_t cycle_count = 0;

    while (cycle_count < max_cycle) {
        uint32_t instr = 0;
        for (int j = 0; j < 4; j++) {
            instr |= ((uint32_t)mem[pc + j] << (j * 8)) & (0xFF << (j * 8));
        }
        printf("%08x : %08x : ", pc, instr);

        int instr_valid = 0;
        instr_valid = decode_rv32i_instr(instr);

        if (instr_valid == 0) {
            instr_valid = decode_rv32m_instr(instr);
        }

        if (instr_valid == 0) {
            instr_valid = decode_rvv_instr(instr);
        }

        if (instr_valid == 0) {
            debug("unknown : instr = 0x%08x\n", instr);
            pc = pc + 4;  
        }
        printf("--------------------\n");
        cycle_count++;
    }

    return -1; // Indicate that the program has not finished
}
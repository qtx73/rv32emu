#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "rv32.h"

uint32_t pc;       // Program counter
uint32_t xreg[32]; // Register file
uint8_t mem[1 << 24]; // Memory

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

    int MAX = 800;
    pc = 0;
    uint32_t time = 0;

    while (time < MAX) {
        printf("%08x: ", pc);
        uint32_t instr = 0;
        for (int j = 0; j < 4; j++) {
            instr |= ((uint32_t)mem[pc + j] << (j * 8)) & (0xFF << (j * 8));
        }
        decode_rvv_instr(instr);
        decode_rv32i_instr(instr);
        printf("--------------------\n");
        time++;
    }

    return -1;
}
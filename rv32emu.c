#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

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
            instr |= (uint32_t)buf[i + j] << (j * 8);
        }
        printf("%08x\n", instr);
    }

    return 0;
}
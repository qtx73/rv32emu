#!/bin/bash

riscv64-unknown-elf-gcc -march=rv32imv -mabi=ilp32 -nostartfiles -T link.ld -o program.elf start.s $1
riscv64-unknown-elf-objcopy -O binary program.elf program.bin


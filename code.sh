#!/bin/bash

#riscv64-unknown-elf-gcc -march=rv32imv -mabi=ilp32 -nostartfiles -O2 -T link.ld -o program.elf start.s $1

CC=/usr/local/opt/llvm/bin/clang
$CC --target=riscv32-unknown-elf -march=rv32imv -mabi=ilp32d -O2 -nostdlib -ffreestanding -T link.ld -o program.elf start.s $1
riscv64-unknown-elf-objcopy -O binary program.elf program.bin


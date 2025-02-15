.section .text
.global _start
_start:
  addi sp, x0, 1024
  call main
  add  gp, x0, a0
  ecall


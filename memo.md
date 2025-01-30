# rvv

0xC0 VL
0xC1 VTYPE

vsetvli 
31-20 imm_i[11:0]
19-15 rs1
14-12 111
11-7  rd
6-0 1010111

vtype
31-8 0
7   vma
6   vta
5-3 vsew
2-0 vlmul

vlen : device dependent. vector length

sew = 8 * (2^vsew) = 8 * (1 << vsew)

vlmul[2:0]
100 reserved
101 lmul=1/8
110 lmul=1/4
111 lmul=1/2
000 lmul=1
001 lmul=2
010 lmul=4
011 lmul=8

vlmax = vlen / sew * lmul

instruction
vsetvli rd, rs1, vtypei # rd = new vl, rs1 = avl, vtypei = new vtype setting

rd, rs1, avl value, effect on vl
-, !x0, x[rs1], normal stripmining
!x0, x0, ~0, set vl to vlmax
x0, !x0, vl, keep vl
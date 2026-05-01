.text
.globl _start

_start:
    lui t1, 0x1
    addi t1, t1, 0x200
    lw t2, 0(t1)
    addi t1, t1, 0x400
    lw t2, 0(t1)
    addi t1, t1, 0x400
    lw t2, 0(t1)
    addi t1, t1, 0x400
    lw t2, 0(t1)
    addi t1, t1, -2048
    addi t1, t1, -1024
    lw t2, 0(t1)
    addi t1, t1, 0x400
    lw t2, 0(t1)
    addi t1, t1, 0x400
    lw t2, 0(t1)
    addi t1, t1, 0x400
    addi t1, t1, 0x400
    lw t2, 0(t1)
    addi t0, zero, 21
loop:
    addi t0, t0, -1
    bne t0, zero, loop
    jalr zero, 0(ra)


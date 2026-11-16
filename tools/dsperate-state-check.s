@ The ARM9 program of tools/dsperate-state-check.c: the count at 0x02100000
@ set to 0, then one more at each VBlank, seen on DISPSTAT. The words in that
@ file are this, assembled: llvm-mc --triple=armv5te -filetype=obj, then the
@ .text.
.syntax unified
.arm
.text
start:
    mov   r0, #0x02100000       @ the VBlank count
    mov   r1, #0
    str   r1, [r0]
    ldr   r2, =0x04000004       @ DISPSTAT
loop:
1:  ldrh  r3, [r2]              @ out of VBlank
    tst   r3, #1
    bne   1b
2:  ldrh  r3, [r2]              @ into VBlank
    tst   r3, #1
    beq   2b
    ldr   r1, [r0]
    add   r1, r1, #1
    str   r1, [r0]
    b     loop
    .ltorg

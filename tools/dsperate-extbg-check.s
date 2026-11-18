@ The ARM9 program of tools/dsperate-extbg-check.c: it runs the commands the
@ check writes to main RAM at 0x02100000. The words in that file are this,
@ assembled: llvm-mc --triple=armv5te -filetype=obj, then the .text.
.syntax unified
.arm
.text
@   1 addr value: str   2 addr value: strh   3 addr value: strb
@   4: wait for the next VBlank to start   5 dst src len: copy words   0: stop
start:
    ldr   r0, =0x02100000
next:
    ldr   r1, [r0], #4
    cmp   r1, #1
    beq   w32
    cmp   r1, #2
    beq   w16
    cmp   r1, #3
    beq   w8
    cmp   r1, #4
    beq   vbl
    cmp   r1, #5
    beq   copy
halt:
    b     halt
w32:
    ldr   r2, [r0], #4
    ldr   r3, [r0], #4
    str   r3, [r2]
    b     next
w16:
    ldr   r2, [r0], #4
    ldr   r3, [r0], #4
    strh  r3, [r2]
    b     next
w8:
    ldr   r2, [r0], #4
    ldr   r3, [r0], #4
    strb  r3, [r2]
    b     next
vbl:
    ldr   r2, =0x04000004
v1:
    ldrh  r3, [r2]
    tst   r3, #1
    bne   v1
v2:
    ldrh  r3, [r2]
    tst   r3, #1
    beq   v2
    b     next
copy:
    ldr   r2, [r0], #4
    ldr   r3, [r0], #4
    ldr   r4, [r0], #4
c1:
    ldr   r5, [r3], #4
    str   r5, [r2], #4
    subs  r4, r4, #4
    bgt   c1
    b     next
.ltorg

# The PlayStation program tools/pcsx-memcard-check.c boots: it talks to the
# memory card in slot 1 directly, on the SIO, the way a game's card library
# does, and leaves what it saw in main RAM at 0x80100000 for the check:
#
#   +0   "MCDT", once it runs
#   +4   1 once the power-on flag is read, 2 once the write is done
#   +8   the card's FLAG byte at power-on: 8, a new card
#   +12  FLAG after a sector write: 0, since a write clears "new card"
#   +16  turns of the loop below
#   +20  FLAG at the latest turn
#
# FLAG is the second byte a card answers to 0x81 'R' (or 'W'). Bit 3 is "new
# card": set when a card is put in, cleared by the first write to it. A game
# that sees it has to take the card for another one.
#
# Built with no linker, so nothing in it is relocated (bal, not jal):
#
#   llvm-mc -triple=mipsel-unknown-none -mcpu=mips1 -filetype=obj \
#       -o p.o tools/pcsx-memcard-check.s
#   llvm-objcopy -O binary --only-section=.text p.o p.bin
#
# and p.bin's words are PROGRAM in the check, loaded at 0x80010000.
	.set noreorder
	.set noat
	.text
_start:
	mtc0  $zero, $12            # SR 0: no interrupt, nothing else on the SIO
	nop
	lui   $s0, 0x8010           # what the check reads
	lui   $s1, 0x1f80
	ori   $s1, $s1, 0x1040      # SIO0: pads and cards
	lui   $t0, 0x5444
	ori   $t0, $t0, 0x434d      # "MCDT"
	sw    $t0, 0($s0)
	sw    $zero, 4($s0)
	sw    $zero, 16($s0)
	bal   flag
	nop
	sw    $v0, 8($s0)
	ori   $t0, $zero, 1
	sw    $t0, 4($s0)
	bal   write63
	nop
	bal   flag
	nop
	sw    $v0, 12($s0)
	ori   $t0, $zero, 2
	sw    $t0, 4($s0)
loop:
	bal   flag
	nop
	sw    $v0, 20($s0)
	lw    $t0, 16($s0)
	nop
	addiu $t0, $t0, 1
	sw    $t0, 16($s0)
	lui   $t1, 1                # a pause, 65536 turns
pause:
	addiu $t1, $t1, -1
	bnez  $t1, pause
	nop
	b     loop
	nop

# v0 = FLAG of the card in slot 1
flag:
	ori   $t0, $zero, 3         # TX on, slot 1 selected
	sh    $t0, 10($s1)
	ori   $t0, $zero, 0x81      # a card
	sb    $t0, 0($s1)
	lbu   $t1, 0($s1)
	ori   $t0, $zero, 0x52      # 'R'
	sb    $t0, 0($s1)
	lbu   $v0, 0($s1)           # FLAG
	nop
	sh    $zero, 10($s1)        # deselected: the exchange is over
	jr    $ra
	nop

# Sector 0x3F, the card's write-test frame, written with 128 x 0xA5: what a
# game does to learn the card takes writes. It clears "new card".
write63:
	ori   $t0, $zero, 3
	sh    $t0, 10($s1)
	ori   $t0, $zero, 0x81
	sb    $t0, 0($s1)
	lbu   $t1, 0($s1)
	ori   $t0, $zero, 0x57      # 'W'
	sb    $t0, 0($s1)
	lbu   $t1, 0($s1)           # FLAG
	sb    $zero, 0($s1)
	lbu   $t1, 0($s1)           # 5A
	sb    $zero, 0($s1)
	lbu   $t1, 0($s1)           # 5D
	sb    $zero, 0($s1)         # sector, high
	lbu   $t1, 0($s1)
	ori   $t0, $zero, 0x3f      # sector, low
	sb    $t0, 0($s1)
	lbu   $t1, 0($s1)
	ori   $t0, $zero, 0xa5
	ori   $t2, $zero, 128
data:
	sb    $t0, 0($s1)
	lbu   $t1, 0($s1)
	addiu $t2, $t2, -1
	bnez  $t2, data
	nop
	ori   $t0, $zero, 0x3f      # checksum: 0 ^ 0x3F, the data's own is 0
	sb    $t0, 0($s1)
	lbu   $t1, 0($s1)
	sb    $zero, 0($s1)
	lbu   $t1, 0($s1)           # 5C
	sb    $zero, 0($s1)
	lbu   $t1, 0($s1)           # 5D
	sb    $zero, 0($s1)
	lbu   $t1, 0($s1)           # 47, the end
	nop
	sh    $zero, 10($s1)
	jr    $ra
	nop

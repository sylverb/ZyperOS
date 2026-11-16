# The TrimUI Brick's part of boot.scr, before board/zyperos/bootstate.cmd
# (which slot boots) and board/brick/boot-slot.cmd (the slot's kernel, initramfs
# and device tree): what board/brick/post-image.sh puts in front of them.
#
# The PLLs boot0 starts, at TrimUI's running values (Allwinner's 4.9 kernel
# can't start them itself)
mw.l 0x03001128 0xd1303333; mw.l 0x03001160 0xc0014396; mw.l 0x03001178 0xc001eb85
mw.l 0x03001028 0xa9003100; mw.l 0x03001030 0xa8007403; mw.l 0x03001040 0xa8001803
mw.l 0x03001048 0xa8006203; mw.l 0x03001050 0xa8003701; mw.l 0x03001058 0xa8002301
mw.l 0x03001060 0xa9002401; mw.l 0x03001068 0xa8003101; mw.l 0x03001078 0xa9042701

# The CPU's PLL as the boot loader left it, for Linux to read in
# /proc/device-tree/chosen/brick,pll-cpux
setexpr.l pllcpux *0x03001000

# Where bootstate.cmd reads and writes: the card's boot partition. The Brick's
# U-Boot has no distro boot (u-boot patch 0006), which would set these.
setenv devtype mmc
setenv devnum 0
setenv distro_bootpart 1
# A charger alone never starts the Brick (u-boot patch 0009 powers it off
# again), so no start is a charger's
setenv zy_charge 0

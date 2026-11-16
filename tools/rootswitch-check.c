/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-rootswitch: how the Brick's initramfs reads the kernel's line
 * (plastron/buildroot/package/brick-rootswitch/src/cmdline.h): the slot's
 * root partition the boot script names, and a developer's root update. */
#include <stdio.h>
#include <string.h>

#include "cmdline.h"

static int fails, checks;

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) {
		fails++;
		printf("  FAIL %s\n", what);
	}
}

int main(void)
{
	char dev[32];
	char longer[200];

	ck(root_from_cmdline("console=ttyS0 rdinit=/rdinit zyperos.root=/dev/mmcblk0p4 zyperos.slot=b",
	                     dev, sizeof dev) && !strcmp(dev, "/dev/mmcblk0p4"), "slot b's root, partition 4");
	ck(root_from_cmdline("zyperos.root=/dev/mmcblk0p2\n", dev, sizeof dev) && !strcmp(dev, "/dev/mmcblk0p2"),
	   "slot a's root, first on the line, a newline after");
	ck(!root_from_cmdline("console=ttyS0 rdinit=/rdinit cma=64M", dev, sizeof dev),
	   "no zyperos.root=: the name lookup");
	ck(!root_from_cmdline("xzyperos.root=/dev/mmcblk0p4", dev, sizeof dev),
	   "only a word of its own");
	ck(!root_from_cmdline("zyperos.root=mmcblk0p4", dev, sizeof dev) &&
	   !root_from_cmdline("zyperos.root=/dev/ zyperos.slot=a", dev, sizeof dev) &&
	   !root_from_cmdline("zyperos.root= zyperos.slot=a", dev, sizeof dev),
	   "not a device: the name lookup");
	memset(longer, 'a', sizeof longer);
	memcpy(longer, "zyperos.root=/dev/", 18);
	longer[sizeof longer - 1] = 0;
	ck(!root_from_cmdline(longer, dev, sizeof dev), "a device longer than the buffer: the name lookup");
	ck(update_from_cmdline("cma=64M bootscr=1 rootupdate=1 zyperos.root=/dev/mmcblk0p2"),
	   "rootupdate=1 between words");
	ck(update_from_cmdline("bootscr=1 rootupdate=1"), "rootupdate=1 last");
	ck(!update_from_cmdline("bootscr=1 rootupdate=10") && !update_from_cmdline("bootscr=1 xrootupdate=1") &&
	   !update_from_cmdline("bootscr=1"), "no update unless the word is rootupdate=1");
	if (fails) {
		printf("rootswitch: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("rootswitch: %d checks passed\n", checks);
	return 0;
}

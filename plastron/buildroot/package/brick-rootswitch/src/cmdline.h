/* rootswitch's reading of the kernel's command line, apart so the host can
 * test it (ZyperOS's tools/rootswitch-check.c, make check-rootswitch). */
#ifndef ROOTSWITCH_CMDLINE_H
#define ROOTSWITCH_CMDLINE_H

#include <stddef.h>
#include <string.h>

/* The slot's root partition, zyperos.root=DEVICE, which the boot script
 * (board/brick/boot-slot.cmd) puts on the line for the slot it boots: 1 and
 * the device in out when the line has one, whole, that starts with /dev/
 * and fits; 0 otherwise, and rootswitch finds the root by its name */
static int root_from_cmdline(const char *cmdline, char *out, size_t size)
{
	static const char key[] = "zyperos.root=";
	const char *p = cmdline;
	size_t n;

	while ((p = strstr(p, key))) {
		if (p == cmdline || p[-1] == ' ') {
			p += sizeof key - 1;
			n = strcspn(p, " \t\n");
			if (n <= 5 || n >= size || strncmp(p, "/dev/", 5) != 0)
				return 0;
			memcpy(out, p, n);
			out[n] = 0;
			return 1;
		}
		p += sizeof key - 1;
	}
	return 0;
}

/* rootupdate=1, a word of its own, from the boot script */
static int update_from_cmdline(const char *cmdline)
{
	const char *p = cmdline;

	while ((p = strstr(p, "rootupdate=1"))) {
		if ((p == cmdline || p[-1] == ' ') && (p[12] == 0 || p[12] == ' ' || p[12] == '\n'))
			return 1;
		p += 12;
	}
	return 0;
}

#endif

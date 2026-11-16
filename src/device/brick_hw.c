/* SPDX-License-Identifier: GPL-3.0-only */
/* The TrimUI Brick's facts for the log pack's hardware report
 * (src/hwreport.h): which of its six panels the kernel found (plastron's
 * kernel patch 0012 says it in the kernel's log), its inputs, the PowerVR
 * driver's own line and the GL it gave the launcher, its Wi-Fi chip, how
 * many of its closed files are on the card (plastron's lists of them, which
 * its start checks too), and the start's marks. On the host, the build's
 * desktop, what the host has. */
#include "hwfacts.h"

#include <sys/stat.h>

#ifdef __linux__
/* The closed files: each of the image's lists (size and name a line) against
 * the boot partition's closed/ */
static void closed_count(hw_facts *f)
{
	static const char *const lists[] = { "/usr/share/zyperos/closed/pvr-ge8300.list",
	                                     "/usr/share/zyperos/closed/xr829-firmware.list" };
	char line[160], name[128], path[200];
	long size;
	struct stat st;
	size_t i;

	f->closed_present = f->closed_total = 0;
	for (i = 0; i < sizeof lists / sizeof lists[0]; i++) {
		FILE *l = fopen(lists[i], "r");

		while (l && fgets(line, sizeof line, l)) {
			if (sscanf(line, "%ld %127s", &size, name) != 2) continue;
			f->closed_total++;
			snprintf(path, sizeof path, "/boot/closed/%s", name);
			if (!stat(path, &st) && st.st_size == size) f->closed_present++;
		}
		if (l) fclose(l);
	}
}

#endif

void plat_hw_facts(hw_facts *f)
{
	memset(f, 0, sizeof *f);
	f->closed_total = -1;
	hwf_common(f);
#ifdef __linux__
	static char klog[1 << 18];   /* the kernel's whole buffer (LOG_BUF_SHIFT 18) */
	char line[160];

	hwf_klog(klog, sizeof klog);
	if (hw_klog_line(klog, "brick panel: ", line, sizeof line))
		snprintf(f->panel, sizeof f->panel, "%s", line + strlen("brick panel: "));
	if (hw_klog_line(klog, "PVR_K:", line, sizeof line))
		snprintf(f->gpu_kernel, sizeof f->gpu_kernel, "%s", line);
	closed_count(f);
#endif
}

/* SPDX-License-Identifier: GPL-3.0-only */
/* The GKD Pixel 2's facts for the log pack's hardware report
 * (src/hwreport.h): its one panel, its inputs, Panfrost's line about the Mali
 * GPU and the GL it gave the launcher, the Wi-Fi adapter's driver when one is
 * in, and the start's marks. It has no closed files. */
#include "hwfacts.h"

void plat_hw_facts(hw_facts *f)
{
	static char klog[1 << 18];
	char line[160];

	memset(f, 0, sizeof *f);
	f->closed_total = -1;
	snprintf(f->panel, sizeof f->panel, "built-in, 480x640 turned");
	hwf_common(f);
	hwf_klog(klog, sizeof klog);
	if (hw_klog_line(klog, "panfrost", line, sizeof line))
		snprintf(f->gpu_kernel, sizeof f->gpu_kernel, "%s", line);
}

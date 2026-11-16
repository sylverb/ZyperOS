/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_HWFACTS_H
#define ZYPEROS_HWFACTS_H

/* What every device's plat_hw_facts (src/device/<id>_hw.c) reads the same
 * way on Linux: the input devices, the Wi-Fi chip's driver, the start's
 * marks, the kernel's log. Static, as each device file includes it once. */

#include "../hwreport.h"

#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <unistd.h>
#endif

/* A small file's text into out, "" without it */
static inline void hwf_text(const char *path, char *out, size_t n)
{
	FILE *f = fopen(path, "r");
	size_t got = 0;

	if (f) {
		got = fread(out, 1, n - 1, f);
		fclose(f);
	}
	out[got] = 0;
}

static inline void hwf_common(hw_facts *f)
{
	snprintf(f->gpu_user, sizeof f->gpu_user, "%s", hw_gl());
#ifdef __linux__
	{
		static char devices[16384];
		char link[256];
		ssize_t n;

		hwf_text("/proc/bus/input/devices", devices, sizeof devices);
		f->n_inputs = hw_inputs_from(devices, f->inputs, HW_INPUTS);
		n = readlink("/sys/class/net/wlan0/device/driver", link, sizeof link - 1);
		if (n > 0) {
			link[n] = 0;
			snprintf(f->wifi, sizeof f->wifi, "%s", strrchr(link, '/') ? strrchr(link, '/') + 1 : link);
		}
		hwf_text("/tmp/boot-times", f->boot_times, sizeof f->boot_times);
	}
#endif
}

/* The kernel's log as it is now, into out */
static inline void hwf_klog(char *out, size_t n)
{
	hw_klog_text(out, n);
}

#endif

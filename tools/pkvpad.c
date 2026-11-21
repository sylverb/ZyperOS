/* SPDX-License-Identifier: GPL-3.0-only */
/* The pad your PICO-8 reads while portkeys holds the joypad: see pkvpad.h. */
#include "pkvpad.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "diatom_port.h"
#include "pkhot.h"

#ifdef __linux__
#include <linux/uinput.h>
#include <sys/ioctl.h>
#endif

/* The kernel's struct input_event as uinput reads it, written out (as
 * tools/pkkbd.c's), so the check builds on a computer */
typedef struct {
	struct timeval time;
	uint16_t       type, code;
	int32_t        value;
} pkvpad_event;

/* The joypad's buttons but MENU, each as tools/pkhot.h counts it */
static const uint16_t CODES[] = {
	PKHOT_BTN_SOUTH, PKHOT_BTN_EAST, PKHOT_BTN_NORTH, PKHOT_BTN_WEST,
	PKHOT_BTN_TL, PKHOT_BTN_TR, PKHOT_BTN_TL2, PKHOT_BTN_TR2,
	PKHOT_BTN_SELECT, PKHOT_BTN_START,
	PKHOT_BTN_UP, PKHOT_BTN_UP + 1, PKHOT_BTN_UP + 2, PKHOT_BTN_UP + 3,
};
#define NCODES (sizeof CODES / sizeof CODES[0])

int pkvpad_open(const char *path, char *node, size_t n)
{
	if (n) node[0] = '\0';
#ifdef __linux__
	struct uinput_setup us;
	char sys[64] = "", dir[160];
	DIR *d;
	struct dirent *e;
	size_t k;
	int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);

	_Static_assert(sizeof(pkvpad_event) == sizeof(struct input_event), "pkvpad_event is input_event");
	if (fd < 0) {
		fprintf(stderr, "portkeys: no pad for PICO-8 (%s: %s)\n", path, strerror(errno));
		return -1;
	}
	memset(&us, 0, sizeof us);
	us.id.bustype = BUS_VIRTUAL;
	us.id.vendor = PKVPAD_VENDOR;
	us.id.product = PKVPAD_PRODUCT;
	us.id.version = PKVPAD_VERSION;
	snprintf(us.name, sizeof us.name, "%s", PKVPAD_NAME);
	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) goto fail;
	for (k = 0; k < NCODES; k++)
		if (ioctl(fd, UI_SET_KEYBIT, CODES[k]) < 0) goto fail;
	if (ioctl(fd, UI_DEV_SETUP, &us) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) goto fail;
	/* Its node, by the name the kernel gave it: inputN, and its eventM */
	if (ioctl(fd, UI_GET_SYSNAME(sizeof sys), sys) >= 0) {
		snprintf(dir, sizeof dir, "/sys/devices/virtual/input/%s", sys);
		if ((d = opendir(dir))) {
			while ((e = readdir(d)))
				if (!strncmp(e->d_name, "event", 5)) snprintf(node, n, "/dev/input/%s", e->d_name);
			closedir(d);
		}
	}
	fprintf(stderr, "portkeys: a pad for PICO-8, %s\n", node[0] ? node : "its node not found");
	return fd;
fail:
	fprintf(stderr, "portkeys: no pad for PICO-8 (uinput: %s)\n", strerror(errno));
	close(fd);
	return -1;
#else
	(void)path;
	fprintf(stderr, "portkeys: no pad for PICO-8 (no uinput on this system)\n");
	return -1;
#endif
}

static bool put(int fd, int type, int code, int value)
{
	pkvpad_event e;

	memset(&e, 0, sizeof e);
	e.type = (uint16_t)type;
	e.code = (uint16_t)code;
	e.value = value;
	return write(fd, &e, sizeof e) == (ssize_t)sizeof e;
}

bool pkvpad_put(int fd, uint32_t was, uint32_t now)
{
	bool ok = true, any = false;
	size_t k;

	if (fd < 0) return false;
	for (k = 0; k < NCODES; k++) {
		const uint32_t bit = pkhot_bit(CODES[k]);

		if ((was ^ now) & bit) {
			ok = put(fd, 1 /* EV_KEY */, CODES[k], (now & bit) ? 1 : 0) && ok;
			any = true;
		}
	}
	if (any) ok = put(fd, 0 /* EV_SYN */, 0 /* SYN_REPORT */, 0) && ok;
	return ok;
}

void pkvpad_close(int fd)
{
#ifdef __linux__
	if (fd >= 0) ioctl(fd, UI_DEV_DESTROY);
#endif
	if (fd >= 0) close(fd);
}

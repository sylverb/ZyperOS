/* SPDX-License-Identifier: GPL-3.0-only */
/* portkeys's keyboard over your PICO-8: see pkkbd.h. */
#include "pkkbd.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/uinput.h>
#include <sys/ioctl.h>
#endif

/* The kernel's struct input_event as uinput reads it, written out, so the
 * check builds on a computer: a timeval, then type, code and value (24 bytes
 * on the Pixel 2's aarch64, and on a 64-bit computer) */
typedef struct {
	struct timeval time;
	uint16_t       type, code;
	int32_t        value;
} pkkbd_event;

#define PKKBD_EV_SYN 0
#define PKKBD_EV_KEY 1

int pkkbd_open(const char *path)
{
#ifdef __linux__
	struct uinput_setup us;
	int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);

	_Static_assert(sizeof(pkkbd_event) == sizeof(struct input_event), "pkkbd_event is input_event");
	if (fd < 0) {
		fprintf(stderr, "portkeys: no keyboard for CTRL-R (%s: %s)\n", path, strerror(errno));
		return -1;
	}
	memset(&us, 0, sizeof us);
	us.id.bustype = BUS_VIRTUAL;
	us.id.vendor = 0x5a59;              /* "ZY" */
	us.id.product = 0x0008;
	us.id.version = 1;
	snprintf(us.name, sizeof us.name, "ZyperOS keys for PICO-8");
	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 || ioctl(fd, UI_SET_KEYBIT, PKKBD_KEY_LEFTCTRL) < 0 ||
	    ioctl(fd, UI_SET_KEYBIT, PKKBD_KEY_R) < 0 || ioctl(fd, UI_DEV_SETUP, &us) < 0 ||
	    ioctl(fd, UI_DEV_CREATE) < 0) {
		fprintf(stderr, "portkeys: no keyboard for CTRL-R (uinput: %s)\n", strerror(errno));
		close(fd);
		return -1;
	}
	fprintf(stderr, "portkeys: a keyboard for CTRL-R over the program\n");
	return fd;
#else
	(void)path;
	fprintf(stderr, "portkeys: no keyboard for CTRL-R (no uinput on this system)\n");
	return -1;
#endif
}

static bool put(int fd, int type, int code, int value)
{
	pkkbd_event e;

	memset(&e, 0, sizeof e);
	e.type = (uint16_t)type;
	e.code = (uint16_t)code;
	e.value = value;
	return write(fd, &e, sizeof e) == (ssize_t)sizeof e;
}

bool pkkbd_ctrl_r(int fd, void (*nap)(int ms))
{
	bool ok;

	if (fd < 0) return false;
	ok = put(fd, PKKBD_EV_KEY, PKKBD_KEY_LEFTCTRL, 1) && put(fd, PKKBD_EV_SYN, 0, 0) &&
	     put(fd, PKKBD_EV_KEY, PKKBD_KEY_R, 1) && put(fd, PKKBD_EV_SYN, 0, 0);
	if (nap) nap(PKKBD_HOLD_MS);
	/* Let go whatever went down, so no key is left held on a write that
	 * failed half way */
	ok = put(fd, PKKBD_EV_KEY, PKKBD_KEY_R, 0) && put(fd, PKKBD_EV_SYN, 0, 0) && ok;
	ok = put(fd, PKKBD_EV_KEY, PKKBD_KEY_LEFTCTRL, 0) && put(fd, PKKBD_EV_SYN, 0, 0) && ok;
	return ok;
}

void pkkbd_close(int fd)
{
#ifdef __linux__
	if (fd >= 0) ioctl(fd, UI_DEV_DESTROY);
#endif
	if (fd >= 0) close(fd);
}

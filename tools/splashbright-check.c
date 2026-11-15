/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-splashbright: the word plastron's splash lights the panel at
 * (package/splash/src/bright.h, zybright on the kernel's line, from the
 * launcher's theme.txt, src/boottheme.h): a backlight value within the
 * backlight's maximum, and none for anything else, so the kernel's own
 * level stays. */
#include "../plastron/buildroot/package/splash/src/bright.h"

#include <stdio.h>

static int fails;

#define CHECK(cond, what) do { \
	if (cond) printf("  ok   %s\n", what); \
	else { printf("  FAIL %s\n", what); fails++; } \
} while (0)

int main(void)
{
	printf("check-splashbright:\n");
	CHECK(splash_bright_value("74", 255) == 74, "a value within the maximum");
	CHECK(splash_bright_value("255", 255) == 255 && splash_bright_value("1", 255) == 1, "the maximum and the least light");
	CHECK(splash_bright_value("256", 255) == -1, "above the maximum: none");
	CHECK(splash_bright_value("99999999999999999999", 255) == -1, "a number too long to read: none");
	CHECK(splash_bright_value("-", 255) == -1 && splash_bright_value("", 255) == -1 &&
	      splash_bright_value(NULL, 255) == -1, "none kept, an older theme.txt, an older boot.scr: none");
	CHECK(splash_bright_value("0", 255) == -1, "0, no light at all: none");
	CHECK(splash_bright_value("017", 255) == -1 && splash_bright_value("7x", 255) == -1 &&
	      splash_bright_value(" 7", 255) == -1, "a leading zero, a word, a space: none");
	CHECK(splash_bright_value("74", 0) == -1, "a backlight with no maximum: none");
	if (fails) { printf("check-splashbright: %d failed\n", fails); return 1; }
	printf("check-splashbright: all passed\n");
	return 0;
}

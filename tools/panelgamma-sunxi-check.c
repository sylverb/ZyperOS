/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-panelgamma-sunxi: the TrimUI Brick's display's gamma table
 * (src/panelgamma_sunxi.h) against a stand-in /dev/disp, the disp2 driver's
 * plain commands answered here as its dev_disp.c answers them: an unsigned
 * long[4] each, screen 0 in the first word, the table and its bytes in the
 * second and third.
 *
 * The cases: Standard (NULL, and all 0) turns the table off and sends no
 * table; a look sends its table, then turns it on; the table's entries are
 * the shared math's (panelgamma_color_out at the floor: no dimming on the
 * Brick) packed red, green, blue in the TCON's 24 bits; black stays black,
 * and white keeps its brightest channel at 255 under Temperature (with
 * Contrast and Gamma, which keep white); Night Mode alone is a table,
 * warmer (less blue at white); a refused table is never turned on; no
 * display is -1 and asks nothing. And Saturation's steps in the display
 * engine's units. */
#include "../src/panelgamma_sunxi.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond, what) do { \
	if (cond) printf("ok: %s\n", what); \
	else { printf("FAIL: %s\n", what); fails++; } \
} while (0)

/* What the stand-in was asked, in order */
static unsigned long asked[8];
static int nasked, refuse_set;
static unsigned long screen_of[8];
static uint32_t got[PANELGAMMA_SUNXI_ENTRIES];
static unsigned long got_bytes;

static int fake_disp(int fd, unsigned long request, void *arg)
{
	unsigned long *a = arg;

	(void)fd;
	if (nasked < 8) {
		screen_of[nasked] = a[0];
		asked[nasked++] = request;
	}
	switch (request) {
	case PANELGAMMA_SUNXI_SET_TABLE:
		if (refuse_set) return -1;
		got_bytes = a[2];
		/* dev_disp.c copies at most LCD_GAMMA_TABLE_SIZE, 256 u32 */
		memcpy(got, (const void *)(uintptr_t)a[1],
		       a[2] > sizeof got ? sizeof got : a[2]);
		return 0;
	case PANELGAMMA_SUNXI_TABLE_ON:
	case PANELGAMMA_SUNXI_TABLE_OFF:
		return 0;
	}
	return -1;
}

static void reset(void)
{
	nasked = 0;
	refuse_set = 0;
	got_bytes = 0;
	memset(got, 0, sizeof got);
	memset(asked, 0, sizeof asked);
	memset(screen_of, 0xff, sizeof screen_of);
}

/* Whether every entry of `got` is c's at the floor, packed */
static int table_is(const panelgamma_color *c)
{
	int i, ch;

	for (i = 0; i < PANELGAMMA_SUNXI_ENTRIES; i++) {
		if (got[i] >> 24) return 0;
		for (ch = 0; ch < 3; ch++)
			if ((int)(got[i] >> (16 - 8 * ch) & 0xff) != panelgamma_color_out(PANELGAMMA_FLOOR, c, ch, i))
				return 0;
	}
	return 1;
}

int main(void)
{
	panelgamma_color std = { 0, 0, 0, 0, 0, 0 };
	panelgamma_color warm = { 4, -2, 3, -1, 0, 0 };
	panelgamma_color night = { 0, 0, 0, 0, 6, 0 };
	panelgamma_color cool = { -10, 0, 0, 0, 0, 0 };
	panelgamma_color one = { 4, 0, 3, -1, 0, 0 };

	panelgamma_ioctl = fake_disp;

	reset();
	CHECK(panelgamma_sunxi_apply(3, NULL) == 0 && nasked == 1 &&
	      asked[0] == PANELGAMMA_SUNXI_TABLE_OFF && screen_of[0] == 0,
	      "Standard (NULL): the table off on screen 0, nothing else");
	reset();
	CHECK(panelgamma_sunxi_apply(3, &std) == 0 && nasked == 1 && asked[0] == PANELGAMMA_SUNXI_TABLE_OFF,
	      "Standard (all 0): the same");

	reset();
	CHECK(panelgamma_sunxi_apply(3, &warm) == 0 && nasked == 2 &&
	      asked[0] == PANELGAMMA_SUNXI_SET_TABLE && asked[1] == PANELGAMMA_SUNXI_TABLE_ON &&
	      screen_of[0] == 0 && screen_of[1] == 0 && got_bytes == 1024,
	      "a look: its table (1024 bytes) on screen 0, then the table on");
	CHECK(table_is(&warm), "  ...every entry the shared math's at the floor, red 23-16, green 15-8, blue 7-0");
	CHECK(got[0] == 0, "  ...black stays black");
	{
		int i, rising = 1;

		for (i = 1; i < PANELGAMMA_SUNXI_ENTRIES; i++)
			if ((got[i] >> 8 & 0xff) < (got[i - 1] >> 8 & 0xff)) rising = 0;
		CHECK(rising, "  ...green never falls as the code value rises");
	}

	reset();
	CHECK(panelgamma_sunxi_apply(3, &one) == 0 && table_is(&one) &&
	      (got[255] >> 16 & 0xff) == 255 && (got[255] & 0xff) < 255,
	      "Temperature with Contrast and Gamma: white keeps its brightest channel (red) at 255");

	reset();
	CHECK(panelgamma_sunxi_apply(3, &night) == 0 && nasked == 2 && asked[1] == PANELGAMMA_SUNXI_TABLE_ON &&
	      table_is(&night) && (got[255] & 0xff) < (got[255] >> 16 & 0xff),
	      "Night Mode alone: a table, less blue than red at white");

	reset();
	CHECK(panelgamma_sunxi_apply(3, &cool) == 0 && table_is(&cool) &&
	      (got[255] >> 16 & 0xff) < (got[255] & 0xff),
	      "Temperature -10: less red than blue at white");

	reset();
	refuse_set = 1;
	CHECK(panelgamma_sunxi_apply(3, &warm) == -1 && nasked == 1 && asked[0] == PANELGAMMA_SUNXI_SET_TABLE,
	      "a table the kernel refuses: -1, and the table never turned on");

	reset();
	CHECK(panelgamma_sunxi_apply(-1, &warm) == -1 && nasked == 0, "no display: -1, nothing asked");

	CHECK(panelgamma_sunxi_saturation(0) == 50, "Saturation 0: 50, the picture as drawn");
	CHECK(panelgamma_sunxi_saturation(10) == 75 && panelgamma_sunxi_saturation(-10) == 25,
	      "Saturation +10 and -10: 75 and 25 (one and a half, half)");
	CHECK(panelgamma_sunxi_saturation(1) == 53 && panelgamma_sunxi_saturation(-1) == 47 &&
	      panelgamma_sunxi_saturation(2) == 55 && panelgamma_sunxi_saturation(-2) == 45,
	      "Saturation's odd steps a half away from 50, its even ones exact");
	CHECK(panelgamma_sunxi_saturation(40) == 75 && panelgamma_sunxi_saturation(-40) == 25,
	      "Saturation out of range: put in range");

	if (fails) {
		printf("check-panelgamma-sunxi: %d failed\n", fails);
		return 1;
	}
	printf("check-panelgamma-sunxi: all passed\n");
	return 0;
}

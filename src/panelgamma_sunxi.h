/* SPDX-License-Identifier: GPL-3.0-only OR MPL-2.0 */
/* The TrimUI Brick's display's gamma table: its Screen Colors and Night Mode.
 *
 * The A133's LCD timing controller (TCON) has a 256-entry table between the
 * display engine and the panel, one 24-bit entry a code value: red in bits
 * 23-16, green 15-8, blue 7-0 (the Brick's kernel, Allwinner's disp2 driver:
 * de/lowlevel_v2x/de_lcd.c's tcon_gamma). The driver takes it from /dev/disp
 * with three of its plain commands, an unsigned long[4] each, screen 0 in the
 * first word as for brightness (include/video/sunxi_display2.h):
 *
 *   0x10b DISP_LCD_SET_GAMMA_TABLE           [1] the table, [2] its bytes
 *   0x10c DISP_LCD_GAMMA_CORRECTION_ENABLE
 *   0x10d DISP_LCD_GAMMA_CORRECTION_DISABLE
 *
 * A table set is kept by the driver and written at the next refresh, in its
 * vblank interrupt (panel_extend_dirty), so a change never tears; it stays
 * for every program after, and through the display going off and on, until
 * one is set again. The device tree starts with the table off (lcd_gamma_en
 * 0), which is Standard.
 *
 * So the table's math is the Pixel 2's (panelgamma.h: Temperature, Tint,
 * Contrast, Gamma and Night Mode's warmth, per channel), with nothing under
 * a floor: the Brick's backlight goes as low as its rungs need, so its table
 * is the one the Pixel 2 has from its floor up.
 *
 * Header only, so a device build that has no such display links none of it.
 * The launcher's Brick file (src/device/brick.c) and Diatom's Brick port
 * include it; Diatom carries a copy, diatom/port/panelgamma_sunxi.h, checked
 * against this one by diatom/tools/check-copies.py. */
#ifndef PANELGAMMA_SUNXI_H
#define PANELGAMMA_SUNXI_H

#include "panelgamma.h"

#include <stdint.h>

#define PANELGAMMA_SUNXI_SET_TABLE 0x10b
#define PANELGAMMA_SUNXI_TABLE_ON  0x10c
#define PANELGAMMA_SUNXI_TABLE_OFF 0x10d
#define PANELGAMMA_SUNXI_ENTRIES   256

/* The table for c (NULL: Standard) in the TCON's form */
static inline void panelgamma_sunxi_table(const panelgamma_color *c,
                                          uint32_t out[PANELGAMMA_SUNXI_ENTRIES])
{
	int i;

	for (i = 0; i < PANELGAMMA_SUNXI_ENTRIES; i++)
		out[i] = (uint32_t)panelgamma_color_out(PANELGAMMA_FLOOR, c, 0, i) << 16 |
		         (uint32_t)panelgamma_color_out(PANELGAMMA_FLOOR, c, 1, i) << 8 |
		         (uint32_t)panelgamma_color_out(PANELGAMMA_FLOOR, c, 2, i);
}

/* c on the display through fd, /dev/disp opened read and write: its table,
 * then the table on; Standard (NULL too) the table off, so the panel shows
 * each frame's own colors. 0 when the kernel took it, -1 when there is no fd
 * or it refused. */
static inline int panelgamma_sunxi_apply(int fd, const panelgamma_color *c)
{
	uint32_t tbl[PANELGAMMA_SUNXI_ENTRIES];
	unsigned long a[4] = { 0, 0, 0, 0 };

	if (fd < 0) return -1;
	if (panelgamma_color_standard(c))
		return panelgamma_ioctl(fd, PANELGAMMA_SUNXI_TABLE_OFF, a) < 0 ? -1 : 0;
	panelgamma_sunxi_table(c, tbl);
	a[1] = (unsigned long)(uintptr_t)tbl;
	a[2] = sizeof tbl;
	if (panelgamma_ioctl(fd, PANELGAMMA_SUNXI_SET_TABLE, a) < 0) return -1;
	return panelgamma_ioctl(fd, PANELGAMMA_SUNXI_TABLE_ON, a) < 0 ? -1 : 0;
}

/* Saturation is not in the table (it mixes the channels): the display
 * engine's own color block has it, in sysfs, 0 to 100 with 50 the picture as
 * drawn and the colors' gain value/50 (de/lowlevel_v2x/de_ccsc.c). Kept by
 * the kernel for every program after, changed at the next refresh. */
#define PANELGAMMA_SUNXI_SATURATION "/sys/class/disp/disp/attr/enhance_saturation"

/* The value for Screen Colors' Saturation step (-PANELGAMMA_RANGE to
 * PANELGAMMA_RANGE, put in range): the Pixel 2's steps, 5% a step, so 25
 * (half) at -10 and 75 (one and a half) at +10, a half step away from 50 */
static inline int panelgamma_sunxi_saturation(int step)
{
	if (step < -PANELGAMMA_RANGE) step = -PANELGAMMA_RANGE;
	if (step > PANELGAMMA_RANGE) step = PANELGAMMA_RANGE;
	return 50 + (5 * step + (step < 0 ? -1 : 1)) / 2;
}

#endif

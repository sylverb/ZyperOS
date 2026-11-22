/* The Border's two makings (src/border.h): the blurred picture from frames
 * of each format, cut to the surface's shape, darkened and opaque; and the
 * DTOV file read whole, its alpha made opaque, any other file refused.
 * Offline: src/border.c alone. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "border.h"

static int g_fail, g_pass;

static void ck(int cond, const char *what)
{
	if (cond) { g_pass++; return; }
	g_fail++;
	printf("  FAIL %s\n", what);
}

#define FW 160
#define FH 144
static uint16_t f565[FW * FH];
static uint32_t f888[FW * FH];
static uint8_t out[BORDER_BLUR_W * BORDER_BLUR_H * 4];

static const uint8_t *cell(int x, int y) { return out + (y * BORDER_BLUR_W + x) * 4; }

int main(void)
{
	int i, x, y, ok;
	char path[] = "/tmp/border-test-XXXXXX";
	int fd;

	/* One color, both formats: the same picture, darker, opaque */
	for (i = 0; i < FW * FH; i++) {
		f565[i] = (uint16_t)((31 << 11) | (0 << 5) | 0);   /* red */
		f888[i] = 0x00ff0000u;                               /* red */
	}
	ck(border_blur(f565, FW, FH, FW * 2, true, 640, 480, out), "an RGB565 frame");
	ok = 1;
	for (i = 0; i < BORDER_BLUR_W * BORDER_BLUR_H; i++)
		if (out[i * 4 + 3] != 255 || out[i * 4 + 2] < 150 || out[i * 4 + 2] > 175 || out[i * 4 + 1] ||
		    out[i * 4]) ok = 0;
	ck(ok, "red, darkened to about 0.55, opaque, BGRA, every cell");
	{
		uint8_t first[sizeof out];

		memcpy(first, out, sizeof out);
		ck(border_blur(f888, FW, FH, FW * 4, false, 640, 480, out) && !memcmp(first, out, sizeof out),
		   "the XRGB8888 frame of the same picture makes the same blur");
	}

	/* The frame's middle in the surface's shape: a 160x144 frame on a 4:3
	 * surface keeps all 160 across and the middle 120 rows. Top and bottom
	 * 12 rows white: cut off, so no cell is lighter than the rest */
	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++)
			f888[y * FW + x] = (y < 12 || y >= FH - 12) ? 0x00ffffffu : 0x00000080u;
	border_blur(f888, FW, FH, FW * 4, false, 640, 480, out);
	ok = 1;
	for (i = 0; i < BORDER_BLUR_W * BORDER_BLUR_H; i++)
		if (out[i * 4 + 1] > 2 || out[i * 4 + 2] > 2) ok = 0;
	ck(ok, "rows outside the surface's shape are left out (no white reaches a cell)");

	/* Left half green, right half blue: the left cells green, the right
	 * blue, and the blur softens the edge between them */
	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++)
			f888[y * FW + x] = x < FW / 2 ? 0x0000ff00u : 0x000000ffu;
	border_blur(f888, FW, FH, FW * 4, false, 640, 480, out);
	ck(cell(0, 12)[1] > 120 && cell(0, 12)[0] < 10, "the left edge green");
	ck(cell(BORDER_BLUR_W - 1, 12)[0] > 120 && cell(BORDER_BLUR_W - 1, 12)[1] < 10, "the right edge blue");
	ck(cell(BORDER_BLUR_W / 2, 12)[1] > 20 && cell(BORDER_BLUR_W / 2, 12)[0] > 20,
	   "and where they meet, some of each: blurred");

	/* None */
	memset(out, 7, sizeof out);
	ck(!border_blur(NULL, FW, FH, FW * 4, false, 640, 480, out) &&
	   !border_blur(f888, 0, FH, FW * 4, false, 640, 480, out) &&
	   !border_blur(f888, FW, FH, FW * 4, false, 0, 480, out) && out[0] == 7,
	   "no frame, no size or no surface: nothing, and the picture left as it was");

	/* A file */
	fd = mkstemp(path);
	if (fd >= 0) {
		uint8_t hdr[8] = { 'D', 'T', 'O', 'V', 3, 0, 2, 0 }, px[3 * 2 * 4];
		int w = 0, h = 0;
		uint8_t *got;

		for (i = 0; i < 6; i++) { px[i * 4] = 10; px[i * 4 + 1] = 20; px[i * 4 + 2] = 30; px[i * 4 + 3] = 0; }
		ck(write(fd, hdr, 8) == 8 && write(fd, px, sizeof px) == (ssize_t)sizeof px, "a scratch file");
		close(fd);
		got = border_read(path, &w, &h);
		ck(got && w == 3 && h == 2 && got[0] == 10 && got[1] == 20 && got[2] == 30 && got[3] == 255 &&
		   got[23] == 255, "a DTOV file read whole, its alpha made opaque");
		free(got);
		truncate(path, 8 + 10);
		ck(!border_read(path, &w, &h), "a file cut short is refused");
		fd = open(path, O_WRONLY | O_TRUNC);
		hdr[0] = 'X';
		ck(write(fd, hdr, 8) == 8, "another");
		close(fd);
		ck(!border_read(path, &w, &h), "a file that is not DTOV is refused");
		fd = open(path, O_WRONLY | O_TRUNC);
		hdr[0] = 'D'; hdr[4] = 0x01; hdr[5] = 0x09;   /* 2305 wide */
		ck(write(fd, hdr, 8) == 8, "and another");
		close(fd);
		ck(!border_read(path, &w, &h), "a picture wider than BORDER_MAX_W is refused");
		unlink(path);
	}
	ck(!border_read(NULL, NULL, NULL) && !border_read("/nonexistent/x.dtov", NULL, NULL), "no file");

	printf("border-test: %d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}

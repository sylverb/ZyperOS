/* SPDX-License-Identifier: GPL-3.0-only */
/* A port's output on its loading screen (DESIGN.md 11.11), checked with
 * plastron's own code for it: splash/src/portout.h, which splash --still
 * draws the line with on the Pixel 2.
 *
 *   portout-check                      the reader and the drawing, on letters
 *                                      made here (make check-portout)
 *   portout-check --atlas FILE         a file the launcher wrote
 *                                      (--portload with ZYPEROS_PORTLOAD_OUT):
 *                                      it reads, its box is on the screen's
 *                                      foot clear of Zyper's frame, and it has
 *                                      every printable ASCII letter
 *   portout-check --follow FILE STILL LOG OUT SECONDS
 *                                      splash's ticks without the display: the
 *                                      still (raw, the panel's 480x640, turned,
 *                                      as the launcher saves it), LOG followed
 *                                      from its end every 250 ms for SECONDS,
 *                                      and each change of the line written as
 *                                      OUT-NN.ppm, the screen turned back to
 *                                      640x480, its line on stdout
 *                                      (tools/portout-shots.sh)
 *
 * The reader is a terminal's view of the bytes: a carriage return starts the
 * line again, "\r\n" ends it, backspace, tabs, escape sequences, UTF-8, the
 * lines of ZyperOS's own programs passed over. The drawing restores the box
 * from the screen before each line, so a shorter line leaves nothing of a
 * longer one, and cuts a line wider than the box at a whole letter with an
 * ellipsis inside the box. Turned, a landscape pixel (x, y) is the panel's
 * row W - 1 - x, column y, as src/device/pixel2.c turns the screen. */
#include "../plastron/buildroot/package/splash/src/portout.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int fails;

#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
	printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Letters made here: every printable ASCII letter, é and the ellipsis, each
 * `adv` wide, its cell a solid block (coverage 255) of its advance, and a
 * box `w` x 4 at (10, 20) on a `lw` x `lh` screen */
static uint8_t made[64 * 1024];
static size_t made_n;

static void made_put16(unsigned v) { made[made_n++] = (uint8_t)v; made[made_n++] = (uint8_t)(v >> 8); }

static void make_atlas(unsigned lw, unsigned lh, unsigned w, unsigned adv)
{
	static const uint32_t extra[] = { 0xE9, PORTOUT_ELLIPSIS };
	unsigned count = 0, h = 4;

	made_n = 0;
	memcpy(made, "ZPOU", 4);
	made_n = 4;
	made_put16(1);
	made_put16(lw);
	made_put16(lh);
	made_put16(10);
	made_put16(20);
	made_put16(w);
	made_put16(h);
	made[made_n++] = 0x10;
	made[made_n++] = 0x20;
	made[made_n++] = 0x30;
	made[made_n++] = 0;
	made_put16(0);
	made_put16(0);           /* the count, below */
	for (uint32_t cp = 0x20; cp < 0x7F + 2; cp++) {
		uint32_t c = cp < 0x7F ? cp : extra[cp - 0x7F];

		made[made_n++] = (uint8_t)c;
		made[made_n++] = (uint8_t)(c >> 8);
		made[made_n++] = (uint8_t)(c >> 16);
		made[made_n++] = (uint8_t)(c >> 24);
		made_put16(adv);
		made_put16(0);
		made_put16(adv);
		memset(made + made_n, c == ' ' ? 0 : 255, (size_t)adv * h);
		made_n += (size_t)adv * h;
		count++;
	}
	made[24] = (uint8_t)count;
	made[25] = (uint8_t)(count >> 8);
}

static portout_atlas A;

/* What the reader shows after `bytes`, as UTF-8 */
static const char *shown_after(portout_reader *R, const char *bytes, size_t n)
{
	static char text[PORTOUT_CHARS * 4 + 1];
	uint32_t s[PORTOUT_CHARS];
	size_t k = 0, len;

	portout_feed(R, &A, (const uint8_t *)bytes, n);
	len = portout_shown(R, &A, s);
	for (size_t i = 0; i < len; i++) {
		uint32_t c = s[i];

		if (c < 0x80) text[k++] = (char)c;
		else if (c < 0x800) { text[k++] = (char)(0xC0 | c >> 6); text[k++] = (char)(0x80 | (c & 0x3F)); }
		else { text[k++] = (char)(0xE0 | c >> 12); text[k++] = (char)(0x80 | (c >> 6 & 0x3F));
		       text[k++] = (char)(0x80 | (c & 0x3F)); }
	}
	text[k] = 0;
	return text;
}

#define SHOWS(R, in, want) do { const char *got_ = shown_after(R, in, sizeof(in) - 1); \
	CHECK(!strcmp(got_, want), "after %s: \"%s\", wanted \"%s\"", #in, got_, want); } while (0)

static void reader_checks(void)
{
	portout_reader R;

	memset(&R, 0, sizeof R);
	SHOWS(&R, "", "");
	SHOWS(&R, "Downloading weston\n", "Downloading weston");
	/* a line being written shows as it is */
	SHOWS(&R, "Unpacking", "Unpacking");
	SHOWS(&R, " data.zip\n", "Unpacking data.zip");
	/* progress rewritten in place: what follows the last \r */
	SHOWS(&R, "  4%\r 12%\r", "12%");
	SHOWS(&R, " 57%", "57%");
	SHOWS(&R, "\r100%\n", "100%");
	/* \r\n is a line end, not an empty line */
	SHOWS(&R, "Done.\r\n", "Done.");
	SHOWS(&R, "\r\n", "Done.");
	/* an empty line, or one of spaces, keeps the last */
	SHOWS(&R, "\n   \t \n", "Done.");
	/* colors and erases dropped, OSC titles too */
	SHOWS(&R, "\033[1;32mOK\033[0m \033[2Kready\n", "OK ready");
	SHOWS(&R, "\033]0;window title\007visible\n", "visible");
	SHOWS(&R, "\033]2;title\033\\after ST\n", "after ST");
	SHOWS(&R, "\033(Bcharset\n", "charset");
	/* an escape left open ends at the line's end */
	SHOWS(&R, "\033[1;3\nafter CSI\n", "after CSI");
	SHOWS(&R, "\033]0;never ended\nafter OSC\n", "after OSC");
	/* a spinner by backspace */
	SHOWS(&R, "Working |\b/\b-", "Working -");
	SHOWS(&R, "\b\\\n", "Working \\");
	/* tabs and runs of spaces are one space; the ends trimmed */
	SHOWS(&R, "  a\t\tb    c  \n", "a b c");
	/* other control characters dropped */
	SHOWS(&R, "be\007ll\001\x7f\n", "bell");
	/* UTF-8: a letter the font has shows, one it has not is dropped, and a
	 * line of nothing it has keeps the last */
	SHOWS(&R, "caf\xc3\xa9 \xe2\x9c\x93 ok\n", "caf\xc3\xa9 ok");
	SHOWS(&R, "\xe4\xb8\xad\xe6\x96\x87\n", "caf\xc3\xa9 ok");
	SHOWS(&R, "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", "caf\xc3\xa9 ok");
	SHOWS(&R, "\n", "caf\xc3\xa9 ok");
	/* a sequence cut short is dropped, and the byte after it read anew */
	SHOWS(&R, "x\xc3y\n", "xy");
	/* split across reads, as the log is read */
	SHOWS(&R, "\xc3", "xy");
	SHOWS(&R, "\xa9t\xc3", "\xc3\xa9t");
	SHOWS(&R, "\xa9\n", "\xc3\xa9t\xc3\xa9");
	SHOWS(&R, "split\r", "split");
	SHOWS(&R, "\nnext\n", "next");
	/* ZyperOS's own lines are not the port's */
	SHOWS(&R, "portkeys: volume 5 -> 6 (VOL+)\n", "next");
	SHOWS(&R, "zyhome: MENU+START\nport: ended, 0\nkernel: battery at start: 81%\n", "next");
	SHOWS(&R, "portkeys: half", "next");
	SHOWS(&R, " a line\nreport: fine\n", "report: fine");
	/* ZyperOS's lines in the middle of the port's progress line, as a Pixel 2
	 * logged them: the port's text before them is the line */
	SHOWS(&R, "Downloading weston\n\r    4%  1.2 MB", "4% 1.2 MB");
	SHOWS(&R, "ssh: [379] Pubkey auth succeeded for 'root'", "4% 1.2 MB");
	SHOWS(&R, " from 10.42.0.2:56201\n", "4% 1.2 MB");
	SHOWS(&R, "\r   18%  5.6 MBmdns: zyperos.local is 10.42.0.1 on usb0\n", "18% 5.6 MB");
	SHOWS(&R, "ssh: [401] Exit (root)\nmdns: let go\n", "18% 5.6 MB");
	SHOWS(&R, "\r  100%portkeys: volume 9 -> 10 (VOL+)\n", "100%");
	/* port: and kernel: only at a line's start, as a port may say them */
	SHOWS(&R, "Starting the port: Celeste\n", "Starting the port: Celeste");
	SHOWS(&R, "Loading kernel: none\n", "Loading kernel: none");
	/* a line longer than the reader keeps: its start */
	{
		char lng[PORTOUT_CHARS + 40];

		memset(lng, 'x', sizeof lng - 1);
		lng[sizeof lng - 2] = '\n';
		lng[sizeof lng - 1] = 0;
		shown_after(&R, lng, strlen(lng));
		CHECK(strlen(shown_after(&R, "", 0)) == PORTOUT_CHARS, "a long line kept %zu letters",
		      strlen(shown_after(&R, "", 0)));
	}
	/* a fresh start of reading forgets the half line, keeps the last */
	SHOWS(&R, "fine\nhalf", "half");
	portout_skip(&R);
	SHOWS(&R, "", "fine");
	SHOWS(&R, "\033[3", "fine");
	portout_skip(&R);
	SHOWS(&R, "1mred\n", "1mred");
}

static int px_ink(uint32_t p) { return (p & 0xffffff) == 0x102030; }

static void draw_checks(void)
{
	uint32_t under[40 * 4], px[40 * 4], s[PORTOUT_CHARS], buf[64 * 64], back[40 * 4];
	portout_reader R;
	size_t n, drawn;
	unsigned x, y, right;

	make_atlas(64, 48, 40, 4);
	CHECK(portout_load(made, made_n, &A) == 0, "the letters made here would not read");
	for (size_t i = 0; i < 40 * 4; i++) under[i] = 0xff000000u | (uint32_t)i;

	/* fits: every letter, ink where each is, the screen elsewhere */
	memset(&R, 0, sizeof R);
	portout_feed(&R, &A, (const uint8_t *)"abc d\n", 6);
	n = portout_shown(&R, &A, s);
	drawn = portout_compose(&A, under, s, n, px);
	CHECK(drawn == 5, "drew %zu of 5", drawn);
	CHECK(px_ink(px[0]) && px_ink(px[11]) && !px_ink(px[12]) && px[12] == under[12] &&
	      px_ink(px[16]) && px[20] == under[20], "\"abc d\" not where its letters are");

	/* too wide: whole letters and the ellipsis, inside the box, no space
	 * before the ellipsis: 9 letters fit before it, the ninth a space */
	memset(&R, 0, sizeof R);
	portout_feed(&R, &A, (const uint8_t *)"abcdefgh ijklmnop\n", 18);
	n = portout_shown(&R, &A, s);
	drawn = portout_compose(&A, under, s, n, px);
	CHECK(drawn == 8, "a line 17 wide in a box of 10: drew %zu letters, wanted 8 and the ellipsis", drawn);
	for (right = 0, x = 0; x < 40; x++)
		if (px_ink(px[x])) right = x + 1;
	CHECK(right == 36, "the ellipsis ends at %u, wanted 36 (8 letters and it)", right);
	CHECK(!px_ink(px[39]), "ink at the box's last column");
	memset(&R, 0, sizeof R);
	portout_feed(&R, &A, (const uint8_t *)"abcdefghijklmnop\n", 17);
	n = portout_shown(&R, &A, s);
	drawn = portout_compose(&A, under, s, n, px);
	CHECK(drawn == 9 && px_ink(px[39]), "a line 16 wide, no spaces: drew %zu, wanted 9 and the ellipsis to the edge",
	      drawn);

	/* a shorter line after a longer: nothing of the longer left */
	memset(&R, 0, sizeof R);
	portout_feed(&R, &A, (const uint8_t *)"ab\n", 3);
	n = portout_shown(&R, &A, s);
	portout_compose(&A, under, s, n, px);
	for (x = 8; x < 40; x++) CHECK(px[x] == under[x], "column %u kept ink after a shorter line", x);

	/* nothing to show: the screen as it was */
	portout_compose(&A, under, s, 0, px);
	CHECK(!memcmp(px, under, sizeof px), "an empty line drew something");

	/* turned: landscape (x, y) is the buffer's row W - 1 - x, column y, and
	 * the box read back is what was put */
	memset(buf, 0, sizeof buf);
	portout_compose(&A, under, s, n, px);
	portout_put_box(&A, px, buf, 48, 1);
	CHECK(buf[(size_t)(64 - 1 - 10) * 48 + 20] == px[0], "turned: landscape (10, 20) not at row 53 column 20");
	CHECK(buf[(size_t)(64 - 1 - 49) * 48 + 23] == px[3 * 40 + 39], "turned: the box's far corner misplaced");
	portout_get_box(&A, buf, 48, 1, back);
	CHECK(!memcmp(back, px, sizeof px), "turned: the box read back is not what was put");
	for (y = 0; y < 64; y++)
		for (x = 0; x < 48; x++) {
			int in = y >= 64 - 1 - 49 && y <= 64 - 1 - 10 && x >= 20 && x < 24;

			if (!in && buf[(size_t)y * 48 + x]) {
				CHECK(0, "turned: wrote outside the box at row %u column %u", y, x);
				y = 64;
				break;
			}
		}
	/* landscape */
	memset(buf, 0, sizeof buf);
	portout_put_box(&A, px, buf, 64, 0);
	portout_get_box(&A, buf, 64, 0, back);
	CHECK(buf[20 * 64 + 10] == px[0] && !memcmp(back, px, sizeof px), "landscape: the box misplaced");

	/* files that do not hold together are refused */
	{
		uint8_t bad[sizeof made];

		CHECK(portout_load(made, 20, &A) < 0, "a cut header read");
		CHECK(portout_load(made, made_n - 1, &A) < 0, "a cut last letter read");
		memcpy(bad, made, made_n);
		bad[0] = 'X';
		CHECK(portout_load(bad, made_n, &A) < 0, "a wrong magic read");
		memcpy(bad, made, made_n);
		bad[14] = 60;            /* w: 10 + 60 > 64 */
		CHECK(portout_load(bad, made_n, &A) < 0, "a box off the screen read");
		memcpy(bad, made, made_n);
		bad[4] = 2;
		CHECK(portout_load(bad, made_n, &A) < 0, "a version 2 file read");
		CHECK(portout_load(made, made_n, &A) == 0, "the good file no longer reads");
	}
}

static uint8_t *slurp(const char *path, size_t *n)
{
	FILE *f = fopen(path, "rb");
	uint8_t *p = NULL;
	long size;

	if (!f) return NULL;
	if (fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 &&
	    (p = malloc((size_t)size)) && fread(p, 1, (size_t)size, f) == (size_t)size)
		*n = (size_t)size;
	else {
		free(p);
		p = NULL;
	}
	fclose(f);
	return p;
}

/* The launcher's file: reads, sits on the foot of 640 x 480 under Zyper's
 * frame (src/portload.c: 504, 528, 192 x 186 units, x 0.625), and has every
 * printable ASCII letter */
static int atlas_check(const char *path)
{
	size_t n = 0;
	uint8_t *p = slurp(path, &n);
	unsigned zyper_bottom = (unsigned)((528 + 186) * 0.625 + 0.5), missing = 0;

	if (!p || portout_load(p, n, &A) < 0) {
		printf("FAIL %s: not a file splash reads\n", path);
		return 1;
	}
	printf("  %s: %u letters, the box %ux%u at %u,%u of %ux%u, ink #%06x\n", path, A.count, A.w, A.h,
	       A.x, A.y, A.land_w, A.land_h, A.ink);
	CHECK(A.land_w == 640 && A.land_h == 480, "the screen %ux%u, not 640x480", A.land_w, A.land_h);
	CHECK(A.y + A.h <= 480 && A.y + A.h > 440, "the box ends at %u, not on the foot", A.y + A.h);
	CHECK(A.w >= 400, "the box only %u wide", A.w);
	for (uint32_t c = 0x21; c < 0x7F; c++)
		if (!portout_glyph_of(&A, c)) missing++;
	CHECK(!missing, "%u printable ASCII letters missing", missing);
	CHECK(portout_glyph_of(&A, PORTOUT_ELLIPSIS) != NULL, "no ellipsis");
	/* The capitals clear of Zyper's frame: H's first row of ink */
	{
		const portout_glyph *g = portout_glyph_of(&A, 'H');
		unsigned top = A.h;

		for (unsigned y = 0; g && y < A.h && top == A.h; y++)
			for (unsigned x = 0; x < g->cw; x++)
				if (g->cov[(size_t)y * g->cw + x] > 127) { top = y; break; }
		CHECK(g && A.y + top >= zyper_bottom, "H's capitals start at %u, inside Zyper's frame (to %u)",
		      A.y + top, zyper_bottom);
	}
	free(p);
	return fails != 0;
}

static double now_s(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

/* The turned panel's buffer as the screen it is, 640 x 480, a PPM */
static int write_ppm(const char *path, const uint32_t *buf)
{
	FILE *f = fopen(path, "wb");

	if (!f) return -1;
	fprintf(f, "P6\n640 480\n255\n");
	for (unsigned y = 0; y < 480; y++)
		for (unsigned x = 0; x < 640; x++) {
			uint32_t p = buf[(size_t)(640 - 1 - x) * 480 + y];

			fputc(p >> 16 & 0xff, f);
			fputc(p >> 8 & 0xff, f);
			fputc(p & 0xff, f);
		}
	return fclose(f);
}

static int follow(const char *atlas, const char *still, const char *log, const char *out, double secs)
{
	size_t an = 0, sn = 0;
	uint8_t *ap = slurp(atlas, &an), *sp = slurp(still, &sn);
	uint32_t *buf = (uint32_t *)sp, *under, *px, now[PORTOUT_CHARS], shown[PORTOUT_CHARS];
	size_t shown_n = 0, k;
	int fd, frame = 0;
	portout_reader R;
	double end;
	char path[1024];

	if (!ap || portout_load(ap, an, &A) < 0 || !sp || sn != 480 * 640 * 4 ||
	    A.land_w != 640 || A.land_h != 480) {
		fprintf(stderr, "portout-check: %s or %s is not what splash reads\n", atlas, still);
		return 2;
	}
	if ((fd = open(log, O_RDONLY)) < 0) {
		fprintf(stderr, "portout-check: %s: %s\n", log, strerror(errno));
		return 2;
	}
	lseek(fd, 0, SEEK_END);
	under = malloc((size_t)A.w * A.h * 4);
	px = malloc((size_t)A.w * A.h * 4);
	portout_get_box(&A, buf, 480, 1, under);
	memset(&R, 0, sizeof R);
	snprintf(path, sizeof path, "%s-%02d.ppm", out, frame++);
	write_ppm(path, buf);
	printf("%s: (nothing yet)\n", path);
	for (end = now_s() + secs; now_s() < end;) {
		uint8_t chunk[4096];
		ssize_t r;
		struct timespec t = { 0, 250 * 1000000L };

		nanosleep(&t, NULL);
		while ((r = read(fd, chunk, sizeof chunk)) > 0) portout_feed(&R, &A, chunk, (size_t)r);
		k = portout_shown(&R, &A, now);
		if (k == shown_n && !memcmp(now, shown, k * sizeof now[0])) continue;
		portout_compose(&A, under, now, k, px);
		portout_put_box(&A, px, buf, 480, 1);
		memcpy(shown, now, k * sizeof now[0]);
		shown_n = k;
		snprintf(path, sizeof path, "%s-%02d.ppm", out, frame++);
		write_ppm(path, buf);
		printf("%s: ", path);
		for (size_t i = 0; i < k; i++) {
			if (now[i] < 0x80) putchar((int)now[i]);
			else printf("\\u%04x", (unsigned)now[i]);
		}
		printf("\n");
		fflush(stdout);
	}
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 3 && !strcmp(argv[1], "--atlas")) return atlas_check(argv[2]);
	if (argc == 7 && !strcmp(argv[1], "--follow"))
		return follow(argv[2], argv[3], argv[4], argv[5], atof(argv[6]));
	make_atlas(640, 480, 400, 4);
	if (portout_load(made, made_n, &A) < 0) {
		printf("FAIL the letters made here would not read\n");
		return 1;
	}
	reader_checks();
	draw_checks();
	if (fails) {
		printf("\n%d FAILED\n", fails);
		return 1;
	}
	printf("ok: the port's output read as a terminal shows it (\\r, \\r\\n, backspace, tabs, escapes,\n"
	       "UTF-8, ZyperOS's own lines passed over, in the middle of a line too), and drawn\n"
	       "over the screen's own box, cut at a whole letter with an ellipsis inside it,\n"
	       "turned as the Pixel 2's panel is\n");
	return 0;
}

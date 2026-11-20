/* SPDX-License-Identifier: GPL-3.0-only */
/* portkeys: the GKD Pixel 2's volume keys while a port runs, and the bar that
 * says what they did.
 *
 * ZyperOS and Diatom read the volume keys themselves, and launch.sh stops both
 * for a port (or a standalone emulator), so for as long as Stardew Valley or
 * OpenBOR ran, VOL+ and VOL- did nothing. launch.sh starts this beside the
 * port and stops it after.
 *
 *   VOL+ / VOL-          the volume, a position at a time
 *   MENU held + VOL      the backlight instead, a rung at a time
 *   MENU on its own      with --menu (OpenBOR, a port), the in-game menu (menu_show)
 *
 * And over any port, "Getting hot" when the chip passes the kernel's trip, as
 * over a game (heat_check), and POWER and a battery run flat (end_port).
 *
 * WHAT A KEY MEANS is tools/pkkeys.c's, which tools/pkkeys-check.c holds:
 * the events of every device are handled in the order of their times, so
 * MENU on the joypad and VOL+ on gpio-keys a few microseconds after it are a
 * brightness step whichever device is read first; the kernel's autorepeat is
 * not a step, and a held key repeats on the launcher's clock instead (300 ms,
 * then every 90, src/keyrepeat.h), woken by poll's timeout; and every step is
 * a line on stderr, which launch.sh sends to the log:
 * "portkeys: volume 5 -> 6 (VOL+)", "portkeys: brightness 7 -> 6 (MENU+VOL-)".
 *
 * THE LAUNCHER'S LEVELS, NOT STEPS OF ITS OWN. The twenty-one volume positions
 * and twelve backlight rungs are src/device/pixel2_levels.h's, turned into
 * register values by the same arithmetic, read from the launcher's settings
 * database when this starts and written back on every step. So the bar shows
 * where you are on the scale the shelf shows, and the shelf comes back at the
 * level you left Stardew at, instead of putting its own back. The gain is
 * written on the RK817's "Master Playback Volume" in the window of the output
 * in use (the jack is watched, as the launcher does) and on "USB Playback
 * Volume" for a USB-C DAC, by ioctl: no alsa-lib, no amixer on the system.
 *
 * AND THE BOOST PAST THE TOP, AS THE LAUNCHER'S (src/boost.h, DESIGN.md 11.6).
 * At the top, VOL+ held a second more or pressed three more times opens cells
 * past it: the volume's are software gain, which this writes to the file the
 * port's ALSA plugin reads every period (src/eqdsp.h's eq_share, the launcher
 * being gone), and the backlight's a lift of the mid-tones in the display's
 * gamma table, loaded on the display borrowed for the bar. The cells are
 * kept beside the levels in the same database, written as this starts, and
 * drawn in the bar every frame while they charge, open, fill or fold;
 * tools/pkkeys.c holds the rules, and a plug in or out closes the volume's.
 *
 * THE BAR, ON THE PANEL'S CURSOR LAYER. The port owns the display: it is the
 * DRM master, and setting any plane is a master's ioctl, so no other process
 * can show anything while it runs - there is no compositor to ask. What this
 * does is borrow the port's own file: pidfd_getfd copies the port's open
 * /dev/dri/card0 into this process (root may), and a copy of a master's file
 * IS master. With it the bar goes up on the VOP's cursor window, above the
 * game, with the legacy cursor ioctl - which the kernel applies as an
 * asynchronous update, outside the commits the port's page flips go through,
 * so the game is neither waited on nor made to wait. Turning the window on
 * as a show starts and off as it ends are ordinary commits, and a flip the
 * port makes in the same refresh as either can be refused, a single repeated
 * frame each. The window is otherwise idle during a port: plastron's SDL
 * hides its pointer when the picture is turned. The VOP cannot turn a plane,
 * so the bar is drawn turned, the way the launcher turns its own picture onto
 * the 480x640 panel. It is drawn in the theme the launcher would be in: the
 * Appearance setting in the launcher's database, with Auto resolved at every
 * press by the launcher's own rule (src/theme.c) in the handheld's zone, so
 * the bar turns at the Night Hours' edges over a port as the shelf would (pkset.c,
 * which tools/portkeys-check.c holds). Only when the database cannot be read
 * is it what the launcher left in /tmp/zyperos-theme (src/themefile.h), which
 * nothing writes again while the port runs; so a hand edit of that file
 * changes the bar over a port only then.
 *
 * A borrowed master is held only while the bar shows. The copy keeps the
 * port's file open, and the master with it, for as long as it is held: a
 * port that closed the display and opened it again, or a second program
 * started by the port's script, or the launcher back from the port, could
 * not become master and would draw nothing. So the copy is let go of after
 * the bar's last frame, and at once if the port's process ends (its pidfd is
 * polled with the keys), and before every frame it is checked to still be
 * the file the port has open (kcmp, which compares the two open files
 * themselves) and still the master, in case the port closed the display or
 * gave the master up while the bar was up. The next press borrows it again.
 * launch.sh waits for this process to end before it starts the launcher.
 *
 * No exclusive grab: the port and gptokeyb still see every key. */
#include <dirent.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/input.h>
#include <linux/kcmp.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "device/pixel2_levels.h"
#include "heat.h"
#include "idle.h"
#include "look.h"
#include "theme.h"
#include "osdbar.h"
#include "panelgamma.h"
#include "p8draw.h"
#include "pkhot.h"
#include "pkkbd.h"
#include "pkkeys.h"
#include "pkset.h"
#include "pkssh.h"
#include "powerhold.h"
#include "portmenu.h"
#include "sidelights.h"
#include "status.h"
#include "themefile.h"

/* TERM, from launch.sh as the port ends: Developer SSH is stopped first
 * (pkssh_stop), its sessions with it, then this ends, as it ended on TERM
 * before. A flag, which the main loop and menu_show's wait look at. */
static volatile sig_atomic_t g_term;
static void on_term(int sig) { (void)sig; g_term = 1; }

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif
#ifndef SYS_pidfd_getfd
#define SYS_pidfd_getfd 438
#endif
#ifndef SYS_kcmp
#define SYS_kcmp 272                 /* asm-generic's, which is aarch64's */
#endif
#ifndef DRM_IOCTL_MODE_GETFB2        /* Linux 5.7 */
#define DRM_IOCTL_MODE_GETFB2 DRM_IOWR(0xCE, struct drm_mode_fb_cmd2)
#endif

/* tools/pkkeys.h writes the kernel's numbers out, to build on the host */
_Static_assert(PK_KEY_VOLUMEUP == KEY_VOLUMEUP && PK_KEY_VOLUMEDOWN == KEY_VOLUMEDOWN &&
               PK_BTN_MENU == BTN_TRIGGER_HAPPY1, "pkkeys.h's key codes are <linux/input.h>'s");

/* An event's time, by the names the kernel's headers have used since 4.16 */
#ifndef input_event_sec
#define input_event_sec  time.tv_sec
#define input_event_usec time.tv_usec
#endif

#define BITS_PER_LONG   (sizeof(long) * 8)
#define NLONGS(n)       (((n) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#define HAS(bits, b)    ((bits[(b) / BITS_PER_LONG] >> ((b) % BITS_PER_LONG)) & 1)

/* The kernel's control structs, vendored as Diatom does (port/pixel2.c) */
struct pk_ctl_elem_id {
	unsigned int numid; int iface; unsigned int device, subdevice;
	unsigned char name[44]; unsigned int index;
};
struct pk_aes_iec958 {
	unsigned char status[24], subcode[147], pad, dig_subframe[4];
};
/* Every member kept, not only the one used: the ioctl number carries the
 * struct's size, and a union one member short is a different ioctl */
struct pk_ctl_elem_value {
	struct pk_ctl_elem_id id;
	unsigned int indirect: 1;
	union {
		union { long value[128]; long *value_ptr; } integer;
		union { long long value[64]; long long *value_ptr; } integer64;
		union { unsigned int item[128]; unsigned int *item_ptr; } enumerated;
		union { unsigned char data[512]; unsigned char *data_ptr; } bytes;
		struct pk_aes_iec958 iec958;
	} value;
	unsigned char reserved[128];
};
#define PK_CTL_ELEM_WRITE  _IOWR('U', 0x13, struct pk_ctl_elem_value)

#define BACKLIGHT  "/sys/class/backlight/backlight/brightness"
#define CARD       "/dev/dri/card0"
#define PANEL_S    0.625f            /* the panel's pixels per launcher unit */

static int mixer = -1, jack = -1;
static pk_levels L = { -1, -1 };     /* positions; -1 not known yet */
/* The boost past the top of each (pkkeys.h): its cells, kept beside the
 * levels, the volume's as the gain in the file the port's plugin reads, the
 * backlight's as the lift in the display's table (port_gamma) */
static pk_boost BO;
static sqlite3 *db;
/* MENU > Appearance as the launcher saved it, "" for never; read once, since
 * nothing can change it while a port runs. appearance_read is whether the
 * database answered at all: if not, the bar goes by THEME_FILE. */
static char appearance_word[16];
static bool appearance_read;

/* The look and Auto's hours from the device's database, as the launcher reads
 * them (src/sysmenu_screen.c, look_load): look.* words and the Night Hours' minutes. The bar's colors
 * for both themes are then the look's (osdbar_colors), or the tokens' for the
 * look as it ships. */
static void look_load_db(sqlite3 *d)
{
	char ac[16] = "", li[16] = "", dk[16] = "", co[16] = "", zy[8] = "", m[16];
	int lm = THEME_NIGHT_UNTIL, dm = THEME_NIGHT_FROM, t;
	look l;

	pkset_text(d, "look.accent", ac, sizeof ac);
	pkset_text(d, "look.light", li, sizeof li);
	pkset_text(d, "look.dark", dk, sizeof dk);
	pkset_text(d, "look.corners", co, sizeof co);
	pkset_text(d, "look.zyper", zy, sizeof zy);
	l = look_parse(ac, li, dk, co, zy);
	look_set(&l);
	for (t = 0; t < DS_THEMES; t++)
		osdbar_colors(t, look_is_default(&l) ? NULL : theme_at(t));
	/* Auto's hours are the Night Hours (src/theme.h): light from
	 * night.until, dark from night.from, as the launcher's night_theme_hours
	 * hands them in. The launcher migrated 1.0.5's auto.light and auto.dark
	 * into them before any port could run. */
	if (pkset_text(d, "night.until", m, sizeof m)) lm = atoi(m);
	if (pkset_text(d, "night.from", m, sizeof m)) dm = atoi(m);
	theme_set_hours(lm, dm);
}

static long now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

/* ---- the levels ------------------------------------------------------------ */

static void ctl_write(const char *name, long raw)
{
	struct pk_ctl_elem_value v;

	if (mixer < 0) return;
	memset(&v, 0, sizeof v);
	v.id.iface = 2;                     /* SNDRV_CTL_ELEM_IFACE_MIXER */
	snprintf((char *)v.id.name, sizeof v.id.name, "%s", name);
	/* Both channels: one written alone reads as a balance problem */
	v.value.integer.value[0] = raw;
	v.value.integer.value[1] = raw;
	ioctl(mixer, PK_CTL_ELEM_WRITE, &v);   /* no DAC, no USB control: fine */
}

static int jack_in(void)
{
	unsigned long sw[NLONGS(SW_MAX + 1)] = { 0 };

	if (jack < 0 || ioctl(jack, EVIOCGSW(sizeof sw), sw) < 0) return 0;
	return HAS(sw, SW_HEADPHONE_INSERT) ? 1 : 0;
}

static void apply_volume(void)
{
	if (L.vol < 0) return;
	ctl_write(GAIN_CTL, px2_gain_raw(L.vol, jack_in()));
	ctl_write(USB_CTL, px2_usb_raw(L.vol));
}

/* A plug in or out, or a resync that read the jack's switch again: the other
 * output's window, as the launcher does (plat_audio_jack_poll), and an output
 * that did change closes the volume boost, so the new one starts at the top
 * without it (src/levels.c's levels_output_changed). The first reading is
 * main's, as the port starts; a resync that finds the plug as it was is no
 * change. */
static int jack_was = -1;

static void plug_changed(void)
{
	int in = jack_in();

	if (jack_was >= 0 && in != jack_was && pk_boost_output_changed(db, &BO))
		fprintf(stderr, "portkeys: the output changed, so the volume boost is closed\n");
	jack_was = in;
	apply_volume();
}

static void apply_brightness(void)
{
	FILE *f = fopen(BACKLIGHT, "w");

	if (!f) return;
	fprintf(f, "%d\n", bright_ladder[L.bright]);
	fclose(f);
}

/* ---- the bar ----------------------------------------------------------------
 *
 * Borrowed for one show: the port's DRM file, its pid and pidfd, the number
 * the port has the file open under (what kcmp compares ours with), and two
 * dumb buffers on it, the bar drawn turned into whichever is not on screen. */

static struct {
	int fd, pidfd, pid, their_fd;
	uint32_t crtc;
	int crtc_index;              /* its place in the card's list, as possible_crtcs counts */
	int pw, ph;                  /* the panel's mode: 480 x 640 */
	int cw;                      /* a buffer's width: bh, padded to 16 pixels */
	uint32_t handle[2], pitch, size;
	uint32_t *map[2];
	uint32_t fb[2];              /* overlay fallback only */
	uint32_t plane;              /* the same */
	int cur;
	bool cursor;                 /* the cursor window took it */
} G = { .fd = -1, .pidfd = -1 };

static struct {
	int kind, level, max;
	long shown, changed;
	bool live;
	int theme;                   /* DS_LIGHT or DS_DARK, as bar_show resolved it at the last press */
	float drawn_a, drawn_rise;   /* what is on the panel, to skip repeats */
	osdbar_look drawn;           /* the same */
	int drawn_theme;
} B;

static int bw, bh;               /* the bar as the player sees it: 373 x 58 */

/* The very ioctl the fallback needs, asked to set plane 0, which there is
 * none of: the master is refused for that (ENOENT), anyone else for not
 * being master (EACCES), before the plane is even looked for. */
static bool is_master(int fd)
{
	struct drm_mode_set_plane p;

	memset(&p, 0, sizeof p);
	return ioctl(fd, DRM_IOCTL_MODE_SETPLANE, &p) == 0 || errno != EACCES;
}

static void release(bool port_gone)
{
	int i;

	if (G.fd < 0) return;
	for (i = 0; i < 2; i++) {
		if (G.map[i]) munmap(G.map[i], G.size);
		G.map[i] = NULL;
	}
	/* The cursor window off, whether the port is still running or not, and
	 * before the file is closed: it is a master's ioctl, so it is done only
	 * while this copy still is the master. Its frame is made inside the kernel
	 * and is no file's, so nothing takes it down with the file, and one left on
	 * the window stays until a master changes it. A port that ends while the
	 * bar is up, still the master, has the window turned off here. One that
	 * gives the master up while the bar shows (SDL's KMSDRM does when its last
	 * window goes, and keeps the file) leaves a frame: held() lets the copy go
	 * and this ioctl is refused. The launcher clears such a frame as it starts,
	 * hiding SDL's pointer (SDL_ShowCursor(SDL_DISABLE), src/device/pixel2.c). */
	if (G.cursor) {
		struct drm_mode_cursor c = { .flags = DRM_MODE_CURSOR_BO, .crtc_id = G.crtc };
		ioctl(G.fd, DRM_IOCTL_MODE_CURSOR, &c);
	}
	/* With the port still running, take the rest of ours down and give the
	 * memory back. With it gone, its file goes with this last copy, and the
	 * kernel frees the buffers and removes the framebuffers ADDFB made, the
	 * overlay's, from their plane. */
	if (!port_gone) {
		if (G.plane && !G.cursor) {
			struct drm_mode_set_plane p = { .plane_id = G.plane, .crtc_id = G.crtc };
			ioctl(G.fd, DRM_IOCTL_MODE_SETPLANE, &p);
		}
		for (i = 0; i < 2; i++) {
			if (G.fb[i]) ioctl(G.fd, DRM_IOCTL_MODE_RMFB, &G.fb[i]);
			if (G.handle[i]) {
				struct drm_mode_destroy_dumb d = { .handle = G.handle[i] };
				ioctl(G.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
			}
		}
	}
	close(G.fd);
	if (G.pidfd >= 0) close(G.pidfd);
	memset(&G, 0, sizeof G);
	G.fd = G.pidfd = -1;
	/* A bar cut short goes away as one that ran its time does: an empty
	 * boost closes (bar_tick). It had no gain and no lift, so nothing is
	 * loaded again for it. */
	if (B.live) pk_boost_gone(db, &BO);
	B.live = false;
	fprintf(stderr, "portkeys: display let go%s\n", port_gone ? " (port ended)" : "");
}

/* Still the file the port has open, and still the master? Else the copy goes.
 *
 * kcmp compares two open files themselves, the same struct file or not, so a
 * port that closed the display, or opened something else under the same
 * number, says no. (Not /proc's fdinfo: the drm-client-id line is printed by
 * a driver's show_fdinfo, and rockchip's file operations, DEFINE_DRM_GEM_FOPS,
 * have none.) And a port can keep the file and give the master up: SDL drops
 * it when its last window goes, and keeps the file. */
static bool held(void)
{
	if (G.fd < 0) return false;
	if (syscall(SYS_kcmp, (pid_t)getpid(), (pid_t)G.pid, KCMP_FILE,
	            (unsigned long)G.fd, (unsigned long)G.their_fd) != 0 || !is_master(G.fd)) {
		release(false);
		return false;
	}
	return true;
}

/* The CRTC lighting the panel, its place in the card's list and its mode. */
static bool find_crtc(int fd)
{
	struct drm_mode_card_res res;
	uint32_t ids[8];
	unsigned i;

	memset(&res, 0, sizeof res);
	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) return false;
	if (res.count_crtcs > 8) res.count_crtcs = 8;
	memset(&res, 0, sizeof res);
	res.count_crtcs = 8;
	res.crtc_id_ptr = (uintptr_t)ids;
	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) return false;
	for (i = 0; i < res.count_crtcs && i < 8; i++) {
		struct drm_mode_crtc c = { .crtc_id = ids[i] };
		if (ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &c) == 0 && c.mode_valid) {
			G.crtc = ids[i];
			G.crtc_index = (int)i;
			G.pw = c.mode.hdisplay;
			G.ph = c.mode.vdisplay;
			return true;
		}
	}
	return false;
}

/* Two ARGB buffers the bar's size, turned: bh wide, bw tall - but the width
 * padded to a multiple of 16 pixels, 64 for the bar's 58.
 *
 * The cursor ioctl takes a width and a height and no pitch: the kernel wraps
 * the buffer as rows of exactly width * 4 bytes (drm_mode_cursor_universal),
 * where rockchip makes a dumb buffer's rows a multiple of 64 bytes, for the
 * Mali. At 58 pixels a row would be 232 bytes to the window and 256 in the
 * buffer, and the bar would lean 24 bytes more on every row. 16 pixels are
 * 64 bytes, so a row of 64 is one the driver has no cause to pad. The six
 * columns past the bar stay clear: they fall on the far side of it from the
 * panel's edge, below it as the player sees it, so it is where it was. */
static size_t notice_bytes(void);

static bool buffers(void)
{
	int i, rows;

	G.cw = (bh + 15) / 16 * 16;
	/* Rows enough for the heat notice as well (notice_put), which goes up on
	 * the same buffers in rows of its own width */
	rows = (int)((notice_bytes() + (size_t)G.cw * 4 - 1) / ((size_t)G.cw * 4));
	if (rows < bw) rows = bw;
	for (i = 0; i < 2; i++) {
		struct drm_mode_create_dumb c = { .width = (uint32_t)G.cw, .height = (uint32_t)rows, .bpp = 32 };
		struct drm_mode_map_dumb m;
		void *p;

		if (ioctl(G.fd, DRM_IOCTL_MODE_CREATE_DUMB, &c) < 0) return false;
		G.handle[i] = c.handle;
		G.pitch = c.pitch;
		G.size = (uint32_t)c.size;
		memset(&m, 0, sizeof m);
		m.handle = c.handle;
		if (ioctl(G.fd, DRM_IOCTL_MODE_MAP_DUMB, &m) < 0) return false;
		p = mmap(NULL, G.size, PROT_READ | PROT_WRITE, MAP_SHARED, G.fd, (off_t)m.offset);
		if (p == MAP_FAILED) return false;
		G.map[i] = p;
		memset(p, 0, G.size);
	}
	return true;
}

static bool overlay_setup(void);

/* A program the in-game menu is starting again (PORT_MENU_RESTART), on its
 * way out: never borrowed from, so no copy of this process's keeps its
 * display, and the master with it, from the loading screen and the program
 * that start next. 0 for none. */
static pid_t gone_pid;

/* Every process but this one with the display open, until one of the copies
 * is the master's: that copy, in G with the pid, its pidfd, the number the
 * port has it under and the CRTC lighting the panel. Nothing made on it yet. */
static bool master_copy(void)
{
	DIR *proc = opendir("/proc");
	struct dirent *pe;
	int self = getpid();

	while (proc && (pe = readdir(proc))) {
		int pid = atoi(pe->d_name), pidfd;
		char dir[64];
		DIR *fds;
		struct dirent *fe;

		if (pid <= 1 || pid == self || pid == gone_pid) continue;
		snprintf(dir, sizeof dir, "/proc/%d/fd", pid);
		if (!(fds = opendir(dir))) continue;
		pidfd = -1;
		while ((fe = readdir(fds))) {
			char link[352], target[64];
			ssize_t n;
			int theirs = atoi(fe->d_name), fd;

			if (fe->d_name[0] == '.') continue;
			snprintf(link, sizeof link, "%s/%s", dir, fe->d_name);
			if ((n = readlink(link, target, sizeof target - 1)) <= 0) continue;
			target[n] = '\0';
			if (strcmp(target, CARD)) continue;
			if (pidfd < 0 && (pidfd = (int)syscall(SYS_pidfd_open, pid, 0)) < 0) break;
			fd = (int)syscall(SYS_pidfd_getfd, pidfd, theirs, 0);
			if (fd < 0) continue;
			if (!is_master(fd) || !find_crtc(fd)) {
				close(fd);
				continue;
			}
			G.fd = fd;
			G.pidfd = pidfd;
			G.pid = pid;
			G.their_fd = theirs;
			closedir(fds);
			closedir(proc);
			return true;
		}
		closedir(fds);
		if (pidfd >= 0) close(pidfd);
	}
	if (proc) closedir(proc);
	return false;
}

/* The port's display, for the bar: its master copy and two buffers on it */
static bool borrow(void)
{
	if (!master_copy()) return false;
	if (!buffers()) {
		fprintf(stderr, "portkeys: no buffers for the bar\n");
		release(false);
		return false;
	}
	/* The cursor window, until it says otherwise - unless the driver padded
	 * the rows after all, which only an overlay plane can be told of (ADDFB2
	 * takes the pitch) */
	G.cursor = G.pitch == (uint32_t)G.cw * 4;
	if (!G.cursor) {
		fprintf(stderr, "portkeys: rows of %u bytes, not %d; using an overlay plane\n",
		        G.pitch, G.cw * 4);
		if (!overlay_setup()) {
			fprintf(stderr, "portkeys: no overlay plane either; no bar\n");
			release(false);
			return false;
		}
	}
	B.drawn_a = -1;     /* nothing of ours on these buffers yet */
	fprintf(stderr, "portkeys: showing the bar over pid %d (%dx%d)\n", G.pid, G.pw, G.ph);
	return true;
}

/* The fallback when the CRTC has no cursor window, or one that will not take
 * the buffer: the first free overlay plane that can be on the panel's CRTC
 * and takes ARGB, with framebuffers of our own. Each update is then an
 * ordinary commit, and can cost the port a frame. */
static bool overlay_setup(void)
{
	struct drm_mode_get_plane_res res = { 0 };
	uint32_t ids[16] = { 0 };
	unsigned i;
	int k;

	res.count_planes = 16;
	res.plane_id_ptr = (uintptr_t)ids;
	if (ioctl(G.fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &res) < 0) return false;
	for (i = 0; i < res.count_planes && i < 16 && !G.plane; i++) {
		uint32_t fmts[32] = { 0 };
		struct drm_mode_get_plane p = { .plane_id = ids[i], .count_format_types = 32,
		                                .format_type_ptr = (uintptr_t)fmts };
		unsigned f;

		/* possible_crtcs counts CRTCs by their place in the card's list */
		if (ioctl(G.fd, DRM_IOCTL_MODE_GETPLANE, &p) < 0 || p.fb_id ||
		    !((p.possible_crtcs >> G.crtc_index) & 1)) continue;
		for (f = 0; f < p.count_format_types && f < 32; f++)
			if (fmts[f] == 0x34325241) G.plane = ids[i];   /* DRM_FORMAT_ARGB8888 */
	}
	if (!G.plane) return false;
	for (k = 0; k < 2; k++) {
		struct drm_mode_fb_cmd2 fb = { .width = (uint32_t)bh, .height = (uint32_t)bw,
		                               .pixel_format = 0x34325241 };
		fb.handles[0] = G.handle[k];
		fb.pitches[0] = G.pitch;
		if (ioctl(G.fd, DRM_IOCTL_MODE_ADDFB2, &fb) < 0) return false;
		G.fb[k] = fb.fb_id;
	}
	return true;
}

/* The bar in its landscape place (x, y), turned onto the portrait panel the
 * way the launcher turns its picture (plat_save_screen: -90 degrees, the
 * landscape top along the panel's left edge): a landscape pixel (x, y) is the
 * panel's (y, W - 1 - x), W the landscape width, the panel's height. False
 * when the frame did not go up, and then the copy may have been let go of. */
static bool put(const osdbar_look *look, float alpha, float rise)
{
	static uint32_t flat[OSDBAR_W * OSDBAR_H];
	int next = G.cur ^ 1, x, y, lx, ly, px, py;
	uint32_t *dst = G.map[next];

	/* In the theme bar_show resolved at the last press, the one the shelf
	 * would be in at that minute, so over a port the bar is the shelf's.
	 * The buffers are osdbar_size's, the boost's room included (buffers). */
	osdbar_render_look(flat, bw, bh, bw, PANEL_S, B.theme, look, alpha, true);
	for (y = 0; y < bh; y++)
		for (x = 0; x < bw; x++)
			dst[(size_t)(bw - 1 - x) * (G.pitch / 4) + y] = flat[(size_t)y * bw + x];

	lx = (int)(OSDBAR_X * PANEL_S + 0.5f);
	ly = (int)((OSDBAR_Y - rise) * PANEL_S + 0.5f);
	px = ly;
	py = G.ph - lx - bw;
	if (G.cursor) {
		/* The whole padded width: the window reads rows of width * 4 */
		struct drm_mode_cursor c = {
			.flags = DRM_MODE_CURSOR_BO | DRM_MODE_CURSOR_MOVE, .crtc_id = G.crtc,
			.x = px, .y = py, .width = (uint32_t)G.cw, .height = (uint32_t)bw,
			.handle = G.handle[next] };
		if (ioctl(G.fd, DRM_IOCTL_MODE_CURSOR, &c) < 0) {
			/* Not master now (EACCES): no plane of any kind would take
			 * it, so the copy goes. No cursor window (ENXIO), or one that
			 * will not take this buffer (EINVAL): an overlay plane, for
			 * the rest of this show. Anything else is this frame's, and
			 * the next one tries the window again. */
			if (errno == EACCES) {
				release(false);
				return false;
			}
			if (errno != ENXIO && errno != EINVAL) return false;
			fprintf(stderr, "portkeys: no cursor window (%s), using an overlay plane\n",
			        strerror(errno));
			G.cursor = false;
			if (!overlay_setup()) {
				fprintf(stderr, "portkeys: no overlay plane either; no bar\n");
				release(false);
				return false;
			}
		}
	}
	if (!G.cursor) {
		struct drm_mode_set_plane p = {
			.plane_id = G.plane, .crtc_id = G.crtc, .fb_id = G.fb[next],
			.crtc_x = px, .crtc_y = py, .crtc_w = (uint32_t)bh, .crtc_h = (uint32_t)bw,
			.src_w = (uint32_t)bh << 16, .src_h = (uint32_t)bw << 16 };
		if (ioctl(G.fd, DRM_IOCTL_MODE_SETPLANE, &p) < 0) {
			if (errno == EACCES) release(false);
			return false;
		}
	}
	G.cur = next;
	return true;
}

static void bar_show(int kind, int level, int max)
{
	long now = now_ms();
	float a, rise;

	if (!held() && !borrow()) return;
	/* The theme is worked out here, on every press, and not once at start:
	 * a port can run for hours, and Auto turns at the Night Hours whether the
	 * launcher is there to say so or not. The setting, read at start, and
	 * the clock, by the launcher's rule (pkset_bar_theme, theme_saved_at); a
	 * word in capitals or none at all reads as the launcher reads it. With no
	 * database to go by, the file the launcher left, read again on every
	 * press: it can be missing on a cold boot straight into a port, and is
	 * then Beard, the default, and not an error. With the database, the file
	 * is not read, so a hand edit of /tmp/zyperos-theme no longer changes the
	 * bar over a port. A change redraws a bar still on the panel too
	 * (bar_tick compares the theme with what it drew), and the next one comes
	 * up in the new theme either way. */
	B.theme = pkset_bar_theme(appearance_read ? appearance_word : NULL, time(NULL),
	                          THEME_FILE);
	/* With no database, the look's colors are the file's too, as the
	 * launcher last wrote them (themefile_read_bar); with one, they were
	 * set at start (look_load_db) */
	if (!appearance_read) {
		ds_theme c;

		if (themefile_read_bar(THEME_FILE, &c) == B.theme) osdbar_colors(B.theme, &c);
	}
	if (!B.live || !osdbar_envelope((uint32_t)(now - B.shown), (uint32_t)(now - B.changed), &a, &rise))
		B.shown = now;
	B.kind = kind;
	B.level = level;
	B.max = max;
	B.changed = now;
	B.live = true;
}

/* Out at once rather than after its hold: POWER let go before its hold was
 * done (pw_bar) */
static void bar_fade(void)
{
	long now = now_ms();

	if (B.live && now - B.changed < OSDBAR_HOLD_MS) B.changed = now - OSDBAR_HOLD_MS;
}

/* ---- "Getting hot" over a port ----------------------------------------------
 *
 * DESIGN.md 11.23's notice with the thermometer, which Diatom shows over a
 * game when the chip passes the kernel's second trip. The launcher renders it
 * as it hands over to the port (src/portmenu.h), in both themes, and here it
 * is read once at the start, scaled to the panel as Diatom scales it (the
 * launcher's 1024 units to 640) and kept. When src/heat.h's rules say so -
 * two readings at 85 C or more, 5 s apart, then quiet until the chip has
 * cooled under 80 C and five minutes have passed - it goes up for
 * DS_MS_HOLD_HEAT where Diatom puts it, at the bottom, centered, on the
 * cursor window the bar uses: the bar, when a volume key is pressed under it,
 * takes the window and gives it back. The chip is read with the battery, every
 * 5 s, so it costs the port nothing it was not already paying. */

static int read_level(const char *path);   /* with the battery, below */

static heat_state H;
static struct {
	uint32_t *px[DS_THEMES];     /* premultiplied ARGB, landscape, panel pixels */
	int w[DS_THEMES], h[DS_THEMES];
	bool live, drawn;            /* up (or under the bar), and on the window now */
	long until;
	int theme;                   /* the theme it went up in */
} N;

/* The chip's temperature in mC, as the launcher reads it (plat_chip_temp):
 * /tmp/zyperos_fake_temp (C or mC) standing in, else the hotter of the
 * soc-thermal and gpu-thermal zones */
static bool chip_temp(int *mc)
{
	static char zone[2][48];
	static int nzones = -1;
	int v = read_level("/tmp/zyperos_fake_temp"), best = -1, i;

	if (v >= 0) {
		*mc = v < 1000 ? v * 1000 : v;
		return true;
	}
	if (nzones < 0) {
		nzones = 0;
		for (i = 0; i < 8 && nzones < 2; i++) {
			char path[64], type[32] = "";
			ssize_t r;
			int fd;

			snprintf(path, sizeof path, "/sys/class/thermal/thermal_zone%d/type", i);
			if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) continue;
			r = read(fd, type, sizeof type - 1);
			close(fd);
			type[r > 0 ? r : 0] = '\0';
			if (strncmp(type, "soc-thermal", 11) && strncmp(type, "gpu-thermal", 11)) continue;
			snprintf(zone[nzones++], sizeof zone[0], "/sys/class/thermal/thermal_zone%d/temp", i);
		}
	}
	for (i = 0; i < nzones; i++)
		if ((v = read_level(zone[i])) > best) best = v;
	if (best < 0) return false;
	*mc = best;
	return true;
}

/* A notice from the launcher's file: scaled by PANEL_S with a bilinear
 * sample of the premultiplied pixels, which keeps the text's edges as
 * Diatom's GPU does; malloc'd, NULL when there is none. With `slot`, the
 * empty square a "SLOT" trailer names (notice.h's NOTICE_SLOT: x, y, side,
 * corner), scaled the same way, or all 0 for none. */
static uint32_t *dtov_read(const char *path, int *pw, int *ph, int slot[4])
{
	FILE *f = fopen(path, "rb");
	uint8_t hdr[8], tr[12];
	uint32_t *src = NULL, *dst = NULL;
	int sw, sh, w, h, x, y;

	if (slot) memset(slot, 0, 4 * sizeof *slot);
	if (!f) return NULL;
	if (fread(hdr, 1, 8, f) != 8 || memcmp(hdr, "DTOV", 4)) goto done;
	sw = hdr[4] | hdr[5] << 8;
	sh = hdr[6] | hdr[7] << 8;
	w = (int)(sw * PANEL_S + 0.5f);
	h = (int)(sh * PANEL_S + 0.5f);
	if (sw <= 0 || sh <= 0 || w < 2 || h < 2 || sw > 1024 || sh > 768) goto done;
	if (!(src = malloc((size_t)sw * sh * 4)) || fread(src, 4, (size_t)sw * sh, f) != (size_t)sw * sh) goto done;
	if (slot && fread(tr, 1, sizeof tr, f) == sizeof tr && !memcmp(tr, "SLOT", 4))
		for (x = 0; x < 4; x++) slot[x] = (int)((tr[4 + x * 2] | tr[5 + x * 2] << 8) * PANEL_S + 0.5f);
	for (x = 0; x < sw * sh; x++) {              /* BGRA bytes are ARGB words: premultiplied */
		uint32_t c = src[x], a = c >> 24;

		src[x] = a << 24 | (((c >> 16) & 255) * a / 255) << 16 | (((c >> 8) & 255) * a / 255) << 8 |
		         ((c & 255) * a / 255);
	}
	if (!(dst = malloc((size_t)w * h * 4))) goto done;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			float fx = (x + 0.5f) / PANEL_S - 0.5f, fy = (y + 0.5f) / PANEL_S - 0.5f;
			int x0 = fx < 0 ? 0 : (int)fx, y0 = fy < 0 ? 0 : (int)fy;
			int x1 = x0 + 1 < sw ? x0 + 1 : x0, y1 = y0 + 1 < sh ? y0 + 1 : y0;
			float tx = fx - x0, ty = fy - y0;
			uint32_t c00 = src[y0 * sw + x0], c10 = src[y0 * sw + x1];
			uint32_t c01 = src[y1 * sw + x0], c11 = src[y1 * sw + x1], out = 0;
			int k;

			if (tx < 0) tx = 0;
			if (ty < 0) ty = 0;
			for (k = 0; k < 32; k += 8) {
				float v = (((c00 >> k) & 255) * (1 - tx) + ((c10 >> k) & 255) * tx) * (1 - ty) +
				          (((c01 >> k) & 255) * (1 - tx) + ((c11 >> k) & 255) * tx) * ty;

				out |= (uint32_t)(v + 0.5f) << k;
			}
			dst[(size_t)y * w + x] = out;
		}
	*pw = w;
	*ph = h;
done:
	free(src);
	fclose(f);
	return dst;
}

/* One theme's heat notice */
static void notice_load(int theme, const char *path)
{
	int w = 0, h = 0;
	uint32_t *px = dtov_read(path, &w, &h, NULL);

	if (!px) return;
	N.px[theme] = px;
	N.w[theme] = w;
	N.h[theme] = h;
}

/* The most either theme's notice needs of a buffer, turned, in rows of its own
 * width padded to 16 pixels, as the bar's are (buffers) */
static size_t notice_bytes(void)
{
	size_t most = 0;
	int t;

	for (t = 0; t < DS_THEMES; t++) {
		size_t n = (size_t)((N.h[t] + 15) / 16 * 16) * 4 * (size_t)N.w[t];

		if (N.px[t] && n > most) most = n;
	}
	return most;
}

/* The notice on the cursor window, where Diatom puts it on the landscape
 * screen: centered, its bottom a 24th of the height up. Turned onto the
 * portrait panel as the bar is (put); as it is on a landscape display (QEMU's).
 * False when it did not go up. */
static bool notice_put(void)
{
	int t = N.theme, w = N.w[t], h = N.h[t], x, y;
	bool turned = G.ph > G.pw;
	int lw = turned ? G.ph : G.pw, lh = turned ? G.pw : G.ph;
	int lx = (lw - w) / 2, ly = lh - h - lh / 24;
	int bw_ = turned ? (h + 15) / 16 * 16 : (w + 15) / 16 * 16, bh_ = turned ? w : h;
	int next = G.cur ^ 1;
	uint32_t *dst = G.map[next];
	struct drm_mode_cursor c;

	if (!N.px[t] || !G.cursor || (size_t)bw_ * 4 * bh_ > G.size) return false;
	memset(dst, 0, (size_t)bw_ * 4 * bh_);
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			uint32_t v = N.px[t][(size_t)y * w + x];

			if (turned) dst[(size_t)(w - 1 - x) * bw_ + y] = v;
			else        dst[(size_t)y * bw_ + x] = v;
		}
	memset(&c, 0, sizeof c);
	c.flags = DRM_MODE_CURSOR_BO | DRM_MODE_CURSOR_MOVE;
	c.crtc_id = G.crtc;
	c.x = turned ? ly : lx;
	c.y = turned ? G.ph - lx - w : ly;
	c.width = (uint32_t)bw_;
	c.height = (uint32_t)bh_;
	c.handle = G.handle[next];
	if (ioctl(G.fd, DRM_IOCTL_MODE_CURSOR, &c) < 0) {
		fprintf(stderr, "portkeys: the heat notice did not go up (%s)\n", strerror(errno));
		return false;
	}
	G.cur = next;
	return true;
}

/* The chip read, with the battery's cadence (heat_poll keeps HEAT_POLL_MS),
 * and the notice up when it is due */
static void heat_check(long now)
{
	heat_poll(&H, (unsigned)now, chip_temp);
	if (!heat_take(&H, (unsigned)now)) return;
	N.theme = pkset_bar_theme(appearance_read ? appearance_word : NULL, time(NULL), THEME_FILE);
	if (!N.px[N.theme]) {
		fprintf(stderr, "portkeys: getting hot (%d mC), but no notice to show\n", H.mc);
		return;
	}
	fprintf(stderr, "portkeys: getting hot (%d mC); the notice over the port\n", H.mc);
	N.live = true;
	N.drawn = false;
	N.until = now + DS_MS_HOLD_HEAT;
}

/* The notice's life, when the bar is not on the window: how long until the
 * next look, -1 for nothing up */
static int notice_tick(long now)
{
	if (!N.live) return -1;
	if (now >= N.until) {
		N.live = false;
		if (N.drawn) release(false);
		N.drawn = false;
		return -1;
	}
	if (!N.drawn) {
		if ((!held() && !borrow()) || !notice_put()) {
			N.live = false;
			release(false);
			return -1;
		}
		N.drawn = true;
		B.drawn_a = -1;      /* the bar's next frame goes up whatever it was */
	} else if (!held()) {
		N.live = N.drawn = false;
		return -1;
	}
	return (int)(N.until - now);
}

/* What the bar shows now: the level's as its boost has it (pkkeys.h), which
 * moves while it charges, opens, fills or folds, and POWER's hold as it was */
static void bar_look(osdbar_look *l, long now)
{
	if (B.kind == OSDBAR_VOLUME || B.kind == OSDBAR_BRIGHT)
		pk_boost_look(&BO, B.kind == OSDBAR_BRIGHT ? PK_BRIGHT : PK_VOLUME, (uint32_t)now, l);
	else
		osdbar_look_plain(l, B.kind, B.level, B.max);
}

static void port_gamma(void);        /* with the keys, below */

/* The bar gone, its time run: an empty boost closes with it and a filled one
 * stays, as in the launcher (levels_bar_gone). Closing an empty one changes
 * neither the gain nor the lift, but the table is asked for all the same, on
 * the display still borrowed; it is left alone when it holds what is wanted. */
static void bar_gone(void)
{
	if (pk_boost_gone(db, &BO) & (1 << PK_BRIGHT)) port_gamma();
}

/* One frame of the bar's life, if it changed; how long until the next. While
 * it slides or fades that is a refresh, 16 ms, and a frame the window did not
 * take is tried again at the next. Every frame first asks whether the copy is
 * still the port's file and still the master. When the bar has gone, a clear
 * frame, then the window off and the copy let go of: the next press borrows
 * it again. Turning the window off and on again is an ordinary commit at each
 * end of a show, where a window kept on with nothing in it would have cost
 * none after the first; but a copy kept between shows keeps the port's
 * master with it (see the top of this file). */
static int bar_tick(void)
{
	long now = now_ms();
	float a, rise;
	bool alive;
	osdbar_look look;

	heat_check(now);
	if (!B.live) return notice_tick(now);
	if (!held()) return -1;
	N.drawn = false;             /* the bar has the window */
	alive = osdbar_envelope((uint32_t)(now - B.shown), (uint32_t)(now - B.changed), &a, &rise);
	if (!alive) a = 0, rise = 0;
	bar_look(&look, now);
	if (a != B.drawn_a || rise != B.drawn_rise || memcmp(&look, &B.drawn, sizeof look) ||
	    B.theme != B.drawn_theme) {
		if (!put(&look, a, rise) && alive) return G.fd < 0 ? -1 : 16;
		B.drawn_a = a;
		B.drawn_rise = rise;
		B.drawn = look;
		B.drawn_theme = B.theme;
	}
	if (!alive) {
		bar_gone();
		/* The heat notice back on the window, if it is still up */
		if (N.live && now < N.until) {
			B.live = false;
			return notice_tick(now);
		}
		release(false);
		return -1;
	}
	/* The boost charging, opening, filling or folding, or VOL+ held on the
	 * top while its hold charges: every frame until it stops, as the
	 * launcher draws it (plat_osd_until) */
	if (B.kind != OSDBAR_POWER && pk_boost_moving(&BO, (uint32_t)now)) return 16;
	{
		uint32_t wait = osdbar_next_change((uint32_t)(now - B.shown), (uint32_t)(now - B.changed));
		return wait ? (int)wait : 16;
	}
}

/* ---- the keys ---------------------------------------------------------------- */

/* The display's gamma table for the rung and the Screen Colors
 * (src/panelgamma.h): the two under the backlight's floor are the table's,
 * 9/17 and 13/17 of the floor's light.
 * Set on the port's display, borrowed for the bar just shown - a master's
 * ioctl, as the bar's are. The launcher set it before the port started and
 * sets it again when it comes back; this is for the steps in between. */
/* Night Mode as the launcher left it (src/nightmode.h): its warmth and
 * nights, read at the start and at each step, and the warmth the table has
 * (night_on): the clock is looked at once a second (night_tick) */
static panelgamma_night night;
static int night_on;
static time_t night_looked;

static void port_gamma(void)
{
	int raw = bright_ladder[L.bright], n;
	panelgamma_color c;

	if (!held()) {
		fprintf(stderr, "portkeys: no display borrowed; the gamma table for brightness %d "
		        "waits for the launcher\n", raw);
		return;
	}
	/* With the player's Screen Colors and Night Mode, as the launcher left
	 * them, and the brightness boost's lift, which is this one's (pkkeys.h) */
	panelgamma_screen_read(PANELGAMMA_COLOR_FILE, &c, &night);
	c.night = night_on = panelgamma_night_at(&night, (long long)time(NULL));
	c.lift = pk_boost_lift(&BO);
	n = panelgamma_apply_color(G.fd, raw, &c);
	if (n > 0)
		fprintf(stderr, "portkeys: the display's gamma table set for brightness %d, lift %d\n", raw,
		        c.lift);
	else if (n < 0)
		fprintf(stderr, "portkeys: could not set the display's gamma table for brightness %d\n",
		        raw);
}

/* Night Mode's hour over the port, once a second: the table for the rung,
 * the Screen Colors and the warmth due now, at once when that warmth is not
 * the one on (one blink, no fade). On the port's display, borrowed for the
 * bar if it is up, else a master's copy taken for the load and let go. */
static void night_tick(void)
{
	time_t now = time(NULL);
	panelgamma_color c;
	bool borrowed;
	int w, n;

	if (now == night_looked) return;
	night_looked = now;
	w = panelgamma_night_at(&night, (long long)now);
	if (w == night_on) return;
	night_on = w;
	borrowed = held();
	if (!borrowed && !master_copy()) {
		fprintf(stderr, "portkeys: Night Mode %s, but no display to load the table on\n",
		        w ? "on" : "off");
		return;
	}
	panelgamma_color_read(PANELGAMMA_COLOR_FILE, &c);
	c.night = w;
	c.lift = pk_boost_lift(&BO);
	n = panelgamma_apply_color(G.fd, bright_ladder[L.bright], &c);
	fprintf(stderr, "portkeys: Night Mode %s%s\n", w ? "on" : "off",
	        n < 0 ? ": could not set the display's gamma table" : "");
	if (!borrowed) release(false);
}

/* Milliseconds to Night Mode's next change, or -1 when none is due */
static int night_wait(void)
{
	long long now = (long long)time(NULL), next = panelgamma_night_next(&night, now);

	if (!next) return -1;
	return next - now > 3600 ? 3600 * 1000 : (int)(next - now) * 1000 + 50;
}

/* The brightness boost kept from before, on the display as portkeys starts.
 * The launcher left its table with the lift as it handed over, so this is for
 * a display that lost it meanwhile; it is left alone when it holds what is
 * wanted. On a master's copy taken for the load and let go, as night_tick's;
 * with no program on the display yet, the next step loads it. */
static void lift_start(void)
{
	panelgamma_color c;
	int n;

	if (!pk_boost_lift(&BO)) return;
	if (!master_copy()) {
		fprintf(stderr, "portkeys: no display yet for the brightness boost's lift %d; the next step "
		        "loads it\n", pk_boost_lift(&BO));
		return;
	}
	panelgamma_color_read(PANELGAMMA_COLOR_FILE, &c);
	c.night = night_on;
	c.lift = pk_boost_lift(&BO);
	n = panelgamma_apply_color(G.fd, bright_ladder[L.bright], &c);
	fprintf(stderr, "portkeys: the brightness boost's lift %d%s\n", c.lift,
	        n < 0 ? ": could not set the display's gamma table" : n ? ", loaded" : ", already on");
	release(false);
}

/* One step from tools/pkkeys.c, through the boost past the top (pk_press):
 * the level, saved when it moved, its line in the log, the hardware and the
 * bar. A press writes the hardware even at an end of the scale, as it always
 * has, which puts the level back if the port moved the mixer itself; a repeat
 * that moved nothing writes nothing, and keeps the bar up. The boost's cells
 * moving are kept, and the volume's gain written, by pk_press; their lift is
 * loaded here, on the display the bar has just borrowed. */
static void press(const pk_step *s)
{
	char line[96];
	int did = pk_press(db, &L, &BO, s, (uint32_t)now_ms(), line, sizeof line);
	bool moved = did & BOOST_LEVEL, vol = s->kind != PK_BRIGHT;

	if (line[0]) fputs(line, stderr);
	if (!vol) {
		if (moved || !s->held) apply_brightness();
		bar_show(OSDBAR_BRIGHT, L.bright, BRIGHT_MAX);
		if (moved || !s->held || (did & BOOST_GAIN)) port_gamma();
	} else {
		if (moved || !s->held) apply_volume();
		bar_show(OSDBAR_VOLUME, L.vol, VOL_MAX);
	}
	/* And on the side lights, as the launcher does (src/sidelights.h) */
	if ((vol ? L.vol : L.bright) >= 0) {
		side_lights_level_line(line, sizeof line, vol, vol ? L.vol : L.bright,
		                       vol ? VOL_MAX : BRIGHT_MAX);
		side_lights_say_to(SIDE_LIGHTS_FIFO, line);
	}
}

/* The boost's clock, on every pass of the loop after its steps
 * (pk_boost_tick): VOL+ let go, a hold on the top charging and, held
 * BOOST_HOLD_MS, opening; presses that stopped draining. A hold that opens it
 * is shown and loaded as a press that does: the bar, and for the backlight
 * the lift; the gain is in the file already. The launcher's rumble as it
 * opens is not given over a port. */
static void boost_clock(const pkkeys *k)
{
	const uint32_t now = (uint32_t)now_ms();
	int i;

	for (i = 0; i < 2; i++) {
		const pk_kind kind = i ? PK_BRIGHT : PK_VOLUME;

		if (!(pk_boost_tick(db, &BO, k, kind, now) & BOOST_GAIN)) continue;
		if (kind == PK_BRIGHT) {
			fprintf(stderr, "portkeys: brightness boost 0 -> %d (MENU+VOL+ held)\n", pk_boost_lift(&BO));
			bar_show(OSDBAR_BRIGHT, L.bright, BRIGHT_MAX);
			port_gamma();
		} else {
			fprintf(stderr, "portkeys: volume boost +0 -> +%d dB (VOL+ held)\n", pk_boost_gain(&BO));
			bar_show(OSDBAR_VOLUME, L.vol, VOL_MAX);
		}
	}
}

/* The battery as the launcher reads it (plat_battery, src/device/pixel2.c):
 * the level from the fuel gauge, and for the charger the cable, not the
 * battery's status, which says Discharging on the cable too; the status only
 * when the charger has no online file. /tmp/zyperos_fake_batt, a level, stands
 * in for both, as it does for the launcher. False with no level to read. */
static int read_level(const char *path)
{
	char buf[16];
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	ssize_t r;

	if (fd < 0) return -1;
	r = read(fd, buf, sizeof buf - 1);
	close(fd);
	if (r <= 0) return -1;
	buf[r] = '\0';
	return atoi(buf);
}

static bool batt_now(int *pct, bool *charging)
{
	char st[16];
	int v = read_level("/tmp/zyperos_fake_batt"), fd;
	ssize_t r;

	*charging = false;
	if (v >= 0) {
		*pct = v;
		return true;
	}
	if ((*pct = read_level("/sys/class/power_supply/battery/capacity")) < 0) return false;
	v = read_level("/sys/class/power_supply/rk817-charger/online");
	if (v == 0 || v == 1) {
		*charging = v == 1;
		return true;
	}
	if ((fd = open("/sys/class/power_supply/battery/status", O_RDONLY | O_CLOEXEC)) >= 0) {
		r = read(fd, st, sizeof st - 1);
		close(fd);
		if (r > 0) {
			st[r] = '\0';
			*charging = !strncmp(st, "Charging", 8) || !strncmp(st, "Full", 4);
		}
	}
	return true;
}

/* ---- the in-game menu, over a port that has one -----------------------------
 *
 * OpenBOR has no libretro core, so it runs as a program of its own, as the
 * standalone Flycast does when a card adds it, and as every port and
 * PortMaster itself do, and the launcher, which draws the in-game menu over a
 * game Diatom runs, has exited for it. With --menu (launch.sh gives it for
 * any "run:" program and any port, src/portmenu.h), MENU on its own
 * (pkkeys_menu_tapped: pressed and let go with no brightness step under it,
 * as Diatom reads it) brings the launcher back for its menu, src/portmenu.h:
 *
 *   1. The port stops where it is, its whole process group (SIGSTOP): the
 *      game is paused, its sound with it, and its last frame stays lit, since
 *      nothing flips it; a helper its script started (gptokeyb) stops with
 *      it. OpenBOR takes a second's gap as one tick of game time
 *      (getinterval), so nothing in the game moved meanwhile.
 *   2. That frame is saved, as the panel shows it, for the menu's backdrop
 *      (still_save), and the port's display is let go of: DROP_MASTER on the
 *      borrowed copy of its file, which root may do for a file it did not
 *      open, as the bar's borrow is (see the top of this file).
 *   3. The launcher starts with --port-menu, takes the display as it takes it
 *      from the boot splash, shows the in-game menu over the saved frame and
 *      exits with what was chosen. It reads the keys itself meanwhile, the
 *      volume ones too, and saves the levels in the same database; this
 *      process waits for it and reads nothing. Over a program MP4 plays on
 *      over (your PICO-8), MP4's follower is stopped meanwhile (--follow,
 *      follow_hold), as SELECT there opens MP4 in the menu.
 *   4. The display goes back to the port (SET_MASTER on the same copy), its
 *      frame is put back on the panel if the launcher left another, what was
 *      pressed for the menu is emptied out of the input queues of every
 *      process in the group (port_input_flush), and it goes on (SIGCONT).
 *      The levels are
 *      read again, and the keys here resynced (in the loop).
 *   5. Quit: launch.sh ends the port, as it ends one for POWER but without
 *      powering off (SIGUSR2), and the shelf comes back. POWER in the menu,
 *      or the battery run flat there: SIGUSR1, as POWER over the port.
 *
 * While the port plays none of this costs it anything: a MENU tap is a key
 * read here like any other, and the port's frames are its own. */

static const char *menu_elf, *menu_tag, *menu_game;   /* --menu ELF TAG GAME */
/* --follow PID: MP4's follower beside a program that keeps MP4 playing
 * (your PICO-8; zyperos.elf --mp4-follow, src/mp4hand.h), stopped while the
 * menu is up, the menu's MP4 being the one that talks to MP4's daemon then
 * (SELECT there), and let go on after it, when it takes the queue the menu
 * left. 0 for every other program. */
static pid_t follow_pid;

static void follow_hold(bool hold)
{
	if (follow_pid > 0) kill(follow_pid, hold ? SIGSTOP : SIGCONT);
}

/* Over your PICO-8 (port_hotkeys): the keyboard for CTRL-R (pkkbd.h), made
 * as this starts; and what the launcher handed over, read again after each
 * menu (keys_load, with the hotkeys below) */
static int kbd = -1;
static void keys_load(void);

static void nap_ms(int ms)
{
	struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };

	nanosleep(&t, NULL);
}

static void end_port(void);          /* with the power key, below */
static void wait_released(int ms);   /* with the keys, below */

/* The process stopped, as /proc says: state T. The name in its stat line can
 * hold spaces and parentheses, so the state is read after the last ')'. */
static bool stopped(int pid)
{
	char path[64], buf[512], *p;
	ssize_t r;
	int fd;

	snprintf(path, sizeof path, "/proc/%d/stat", pid);
	if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) return false;
	r = read(fd, buf, sizeof buf - 1);
	close(fd);
	if (r <= 0) return false;
	buf[r] = '\0';
	p = strrchr(buf, ')');
	return p && p[1] == ' ' && (p[2] == 'T' || p[2] == 't');
}

static bool write_all(int fd, const void *buf, size_t n)
{
	const uint8_t *b = buf;

	while (n) {
		ssize_t w = write(fd, b, n);

		if (w < 0 && errno == EINTR) continue;
		if (w <= 0) return false;
		b += w;
		n -= (size_t)w;
	}
	return true;
}

/* The frame on the panel, framebuffer `fb_id`, as the panel shows it. Its
 * buffer is mapped as the card's own dumb buffer, which Mesa makes a scanout
 * buffer as on this panel, or else as the dma-buf it is; a frame in any other
 * form than plain 32-bit rows is not read, and the menu then sits on the
 * theme's ground instead. Write-combined memory on the Pixel 2: still_save
 * copies its 1.2 MB once, a screenshot reads 128 x 128 of its pixels and no
 * more (shot_take). */
typedef struct {
	const uint8_t *px;           /* the first row */
	uint8_t       *map;
	size_t         size;
	uint32_t       w, h, pitch, handle;
	int            dmabuf;
} frame_map;

static void frame_close(frame_map *m)
{
	if (m->dmabuf >= 0) {
		struct dma_buf_sync sy = { DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ };

		ioctl(m->dmabuf, DMA_BUF_IOCTL_SYNC, &sy);
		close(m->dmabuf);
	}
	if (m->map && m->map != MAP_FAILED) munmap(m->map, m->size);
	/* The handle GETFB2 made, in the port's file: a new one of its own, so
	 * closing it leaves the port's handles to the same buffer as they were */
	if (m->handle) {
		struct drm_gem_close gc = { .handle = m->handle };

		ioctl(G.fd, DRM_IOCTL_GEM_CLOSE, &gc);
	}
	memset(m, 0, sizeof *m);
	m->dmabuf = -1;
}

static bool frame_open(uint32_t fb_id, frame_map *m)
{
	struct drm_mode_fb_cmd2 f;
	struct drm_mode_map_dumb md;
	size_t row;

	memset(m, 0, sizeof *m);
	m->dmabuf = -1;
	memset(&f, 0, sizeof f);
	f.fb_id = fb_id;
	if (ioctl(G.fd, DRM_IOCTL_MODE_GETFB2, &f) < 0 || !f.handles[0]) {
		fprintf(stderr, "portkeys: the port's frame cannot be read (%s)\n", strerror(errno));
		return false;
	}
	m->handle = f.handles[0];
	row = (size_t)f.width * 4;
	if ((f.pixel_format != 0x34325258 && f.pixel_format != 0x34325241) ||   /* XRGB8888, ARGB8888 */
	    ((f.flags & DRM_MODE_FB_MODIFIERS) && f.modifier[0] != 0) ||          /* not linear */
	    !f.width || !f.height || f.width > 4096 || f.height > 4096 || f.pitches[0] < row) {
		fprintf(stderr, "portkeys: the port's frame is format %08x, modifier %llx; not read\n",
		        f.pixel_format, (unsigned long long)f.modifier[0]);
		frame_close(m);
		return false;
	}
	m->size = (size_t)f.offsets[0] + (size_t)f.pitches[0] * f.height;
	m->map = MAP_FAILED;
	memset(&md, 0, sizeof md);
	md.handle = f.handles[0];
	if (ioctl(G.fd, DRM_IOCTL_MODE_MAP_DUMB, &md) == 0)
		m->map = mmap(NULL, m->size, PROT_READ, MAP_SHARED, G.fd, (off_t)md.offset);
	if (m->map == MAP_FAILED) {
		struct drm_prime_handle ph = { .handle = f.handles[0], .flags = DRM_CLOEXEC };

		if (ioctl(G.fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &ph) == 0) {
			m->dmabuf = ph.fd;
			m->map = mmap(NULL, m->size, PROT_READ, MAP_SHARED, m->dmabuf, 0);
		}
	}
	if (m->map == MAP_FAILED) {
		fprintf(stderr, "portkeys: the port's frame cannot be mapped (%s); not read\n", strerror(errno));
		frame_close(m);
		return false;
	}
	if (m->dmabuf >= 0) {
		struct dma_buf_sync sy = { DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ };

		ioctl(m->dmabuf, DMA_BUF_IOCTL_SYNC, &sy);
	}
	m->px = m->map + f.offsets[0];
	m->w = f.width;
	m->h = f.height;
	m->pitch = f.pitches[0];
	return true;
}

/* Saved to PORT_MENU_STILL, for the menu's backdrop */
static bool still_save(uint32_t fb_id)
{
	frame_map m;
	uint8_t *out, hdr[8];
	size_t row;
	uint32_t y;
	int fd;
	bool ok;

	if (!frame_open(fb_id, &m)) return false;
	row = (size_t)m.w * 4;
	if (!(out = malloc(row * m.h))) {
		frame_close(&m);
		return false;
	}
	for (y = 0; y < m.h; y++) memcpy(out + y * row, m.px + (size_t)y * m.pitch, row);
	memcpy(hdr, PORT_MENU_MAGIC, 4);
	hdr[4] = (uint8_t)m.w;  hdr[5] = (uint8_t)(m.w >> 8);
	hdr[6] = (uint8_t)m.h;  hdr[7] = (uint8_t)(m.h >> 8);
	frame_close(&m);
	fd = open(PORT_MENU_STILL ".tmp", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	ok = fd >= 0 && write_all(fd, hdr, sizeof hdr) && write_all(fd, out, row * (size_t)(hdr[6] | hdr[7] << 8));
	if (fd >= 0 && close(fd)) ok = false;
	ok = ok && rename(PORT_MENU_STILL ".tmp", PORT_MENU_STILL) == 0;
	if (!ok) unlink(PORT_MENU_STILL ".tmp");
	free(out);
	return ok;
}

/* The connector the panel's CRTC lights, for putting the port's frame back */
static uint32_t crtc_connector(void)
{
	struct drm_mode_card_res res;
	uint32_t ids[8];
	unsigned i;

	memset(&res, 0, sizeof res);
	res.count_connectors = 8;
	res.connector_id_ptr = (uintptr_t)ids;
	if (ioctl(G.fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) return 0;
	for (i = 0; i < res.count_connectors && i < 8; i++) {
		struct drm_mode_get_connector c = { .connector_id = ids[i] };
		struct drm_mode_get_encoder e;

		if (ioctl(G.fd, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0 || !c.encoder_id) continue;
		memset(&e, 0, sizeof e);
		e.encoder_id = c.encoder_id;
		if (ioctl(G.fd, DRM_IOCTL_MODE_GETENCODER, &e) == 0 && e.crtc_id == G.crtc) return ids[i];
	}
	return 0;
}

/* What was pressed while the menu was up was the launcher's, and the port has
 * it all queued for the moment it goes on: the A that chose Continue would be
 * an attack. So would a helper the port's script started beside it, which
 * reads the pad too, gptokeyb turning it into keys -
 * and would type it all into the game. Each event device any process of the
 * port's group has open has the clock it is read on changed and changed
 * back, which empties that queue and leaves the kernel's SYN_DROPPED in it
 * (evdev_set_clk_type). SDL reads that as events lost and asks the device
 * what is down now, so the port sees the buttons as they are and nothing
 * pressed for the menu; a helper that does not read SYN_DROPPED so (gptokeyb)
 * loses only a release that came while the menu was up. Each is left on the
 * wall clock, the kernel's own default, which SDL does not change. */
static int input_flush_pid(int pid)
{
	char dir[64];
	DIR *fds;
	struct dirent *fe;
	int flushed = 0, pidfd;

	snprintf(dir, sizeof dir, "/proc/%d/fd", pid);
	if (!(fds = opendir(dir))) return 0;
	if ((pidfd = (int)syscall(SYS_pidfd_open, pid, 0)) < 0) {
		closedir(fds);
		return 0;
	}
	while ((fe = readdir(fds))) {
		char link[352], target[64];
		ssize_t r;
		int fd, mono = CLOCK_MONOTONIC, wall = CLOCK_REALTIME;

		if (fe->d_name[0] == '.') continue;
		snprintf(link, sizeof link, "%s/%s", dir, fe->d_name);
		if ((r = readlink(link, target, sizeof target - 1)) <= 0) continue;
		target[r] = '\0';
		if (strncmp(target, "/dev/input/event", 16)) continue;
		if ((fd = (int)syscall(SYS_pidfd_getfd, pidfd, atoi(fe->d_name), 0)) < 0) continue;
		ioctl(fd, EVIOCSCLOCKID, &mono);
		ioctl(fd, EVIOCSCLOCKID, &wall);
		close(fd);
		flushed++;
	}
	close(pidfd);
	closedir(fds);
	return flushed;
}

/* The process group of a process, from its stat line (after the name) */
static int pgrp_of(int pid)
{
	char path[64], buf[512], *p, state;
	int ppid = 0, pg = -1;
	ssize_t r;
	int fd;

	snprintf(path, sizeof path, "/proc/%d/stat", pid);
	if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) return -1;
	r = read(fd, buf, sizeof buf - 1);
	close(fd);
	if (r <= 0) return -1;
	buf[r] = '\0';
	if (!(p = strrchr(buf, ')')) || sscanf(p + 1, " %c %d %d", &state, &ppid, &pg) != 3) return -1;
	return pg;
}

static void port_input_flush(int pg)
{
	DIR *proc = opendir("/proc");
	struct dirent *pe;
	int queues = 0, procs = 0;

	while (proc && (pe = readdir(proc))) {
		int pid = atoi(pe->d_name), n;

		if (pid <= 1 || pgrp_of(pid) != pg) continue;
		if ((n = input_flush_pid(pid)) > 0) procs++;
		queues += n;
	}
	if (proc) closedir(proc);
	fprintf(stderr, "portkeys: %d input queue(s) of %d process(es) emptied of the menu's presses\n",
	        queues, procs);
}

/* MemAvailable, in MB, -1 when /proc does not say */
static long mem_available_mb(void)
{
	char buf[2048], *p;
	int fd = open("/proc/meminfo", O_RDONLY | O_CLOEXEC);
	ssize_t r;

	if (fd < 0) return -1;
	r = read(fd, buf, sizeof buf - 1);
	close(fd);
	if (r <= 0) return -1;
	buf[r] = '\0';
	if (!(p = strstr(buf, "MemAvailable:"))) return -1;
	return atol(p + 13) / 1024;
}

/* MENU tapped: the in-game menu, as at the top of this section. 0 when there
 * was none (no --menu, or no port holding the display); 1 when it was up, and
 * this process's keys and levels are then to be read again. */
/* The port's group while nothing holds the display yet: the child of
 * launch.sh (this process's parent) that leads a group of its own, as the
 * port's script does under setsid. 0 for none. Splash and this process are
 * launch.sh's children too, in its group. */
static pid_t port_group(void)
{
	DIR *proc = opendir("/proc");
	struct dirent *pe;
	pid_t self = getpid(), parent = getppid(), found = 0;

	while (proc && (pe = readdir(proc))) {
		char path[64], line[512], *close_paren;
		int pid = atoi(pe->d_name), ppid = 0, pgrp = 0, fd;
		ssize_t n;

		if (pid <= 1 || pid == self) continue;
		snprintf(path, sizeof path, "/proc/%d/stat", pid);
		if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) continue;
		n = read(fd, line, sizeof line - 1);
		close(fd);
		if (n <= 0) continue;
		line[n] = '\0';
		/* After the name, which may hold spaces and parentheses */
		if (!(close_paren = strrchr(line, ')')) ||
		    sscanf(close_paren + 1, " %*c %d %d", &ppid, &pgrp) != 2)
			continue;
		if (ppid == parent && pgrp == pid) {
			found = pid;
			break;
		}
	}
	if (proc) closedir(proc);
	return found;
}

/* MENU while a port is still starting: no program holds the display yet,
 * and the loading screen splash holds is on the panel. The same in-game menu
 * over that screen, Continue named Keep Waiting (PORT_MENU_WAITING), so a
 * port that hangs as it starts is MENU and Quit away from the shelf, and one
 * that is only slow goes on (1.0.6; before, MENU did nothing until the port
 * drew, and POWER or MENU+START held were the only ways out). The port's
 * group is stopped while the menu is up, as over a running port: one that
 * reached its display meanwhile would find the menu's master in its way.
 * 1 when the menu was up. */
static int menu_waiting(void)
{
	struct drm_mode_crtc c;
	uint32_t conn;
	pid_t pg = port_group(), child;
	int fd, status = 0, how = PORT_MENU_CONTINUE, i;
	long t0 = now_ms();

	if ((fd = open(CARD, O_RDWR | O_CLOEXEC)) < 0) return 0;
	/* Opened while nobody is master (splash let go of it), this file has
	 * become master, and the menu could never take the display: let go.
	 * Nothing below needs it until the menu has ended. */
	ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
	G.fd = fd;
	if (!find_crtc(fd)) {
		G.fd = -1;
		close(fd);
		return 0;
	}
	/* For splash: what the log gets from here on is the menu's, not the
	 * port's (PORT_MENU_UP), until the loading screen is back */
	if ((i = open(PORT_MENU_UP, O_WRONLY | O_CREAT | O_CLOEXEC, 0644)) >= 0) close(i);
	fprintf(stderr, "portkeys: MENU while the port starts (group %d); the menu over its loading screen\n",
	        (int)pg);
	if (pg > 1) kill(-pg, SIGSTOP);
	memset(&c, 0, sizeof c);
	c.crtc_id = G.crtc;
	if (ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &c) < 0) c.fb_id = 0;
	unlink(PORT_MENU_STILL);
	if (c.fb_id) still_save(c.fb_id);
	conn = crtc_connector();

	follow_hold(true);
	child = fork();
	if (child == 0) {
		int o = open("/proc/self/oom_score_adj", O_WRONLY | O_CLOEXEC);

		if (o >= 0) {
			if (write(o, PORT_MENU_OOM, strlen(PORT_MENU_OOM)) < 0) {}
			close(o);
		}
		execl(menu_elf, menu_elf, PORT_MENU_ARG, menu_tag, menu_game, PORT_MENU_WAITING,
		      (char *)NULL);
		_exit(127);
	}
	if (child < 0) fprintf(stderr, "portkeys: cannot start the menu (%s)\n", strerror(errno));
	while (child > 0 && waitpid(child, &status, 0) < 0 && errno == EINTR && !g_term) {}
	follow_hold(false);
	if (child > 0 && WIFEXITED(status) &&
	    (WEXITSTATUS(status) == PORT_MENU_QUIT || WEXITSTATUS(status) == PORT_MENU_POWER ||
	     WEXITSTATUS(status) == PORT_MENU_RESTART))
		how = WEXITSTATUS(status);

	/* The loading screen back on the panel: the launcher's picture went
	 * with it. Set even when the panel still shows the splash's buffer (the
	 * menu could not draw): a display that keeps its own copy of it (QEMU's)
	 * shows what splash wrote meanwhile, its after-a-minute line, only once
	 * set. Nobody holds the master now, so this file can */
	for (i = 0; i < 150 && ioctl(fd, DRM_IOCTL_SET_MASTER, 0) < 0; i++) usleep(2000);
	if (i < 150) {
		if (c.fb_id && c.mode_valid && conn) {
			c.set_connectors_ptr = (uintptr_t)&conn;
			c.count_connectors = 1;
			if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &c) < 0)
				fprintf(stderr, "portkeys: cannot put the loading screen back (%s)\n", strerror(errno));
		}
		ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
	}
	G.fd = -1;
	close(fd);
	wait_released(1500);
	/* The menu has ended: the port's lines again, from when it goes on */
	unlink(PORT_MENU_UP);
	if (pg > 1) {
		port_input_flush(pg);
		kill(-pg, SIGCONT);
	}
	pk_levels_load(db, BACKLIGHT, &L);
	pk_boost_load(db, &L, &BO);
	fprintf(stderr, "portkeys: the menu was up %ld ms while the port started: %s\n", now_ms() - t0,
	        how == PORT_MENU_QUIT ? "Quit" : how == PORT_MENU_POWER ? "POWER" :
	        how == PORT_MENU_RESTART ? "start again" : "Keep Waiting");
	/* Quit, or a start again (a Display mode over your PICO-8): launch.sh
	 * tells the two apart by the restart file the menu wrote (port_quit) */
	if (how == PORT_MENU_QUIT || how == PORT_MENU_RESTART) {
		pid_t parent = getppid();

		if (parent > 1) kill(parent, SIGUSR2);
	} else if (how == PORT_MENU_POWER) {
		end_port();
	}
	return 1;
}

/* `extra` after the tag and the game, NULL for none: PORT_MENU_RESET_ARG for
 * Reset Game's hotkey, which asks its question alone (hot_do) */
static int menu_show_as(const char *extra)
{
	struct drm_mode_crtc c;
	uint32_t conn;
	pid_t pg, child;
	long t0 = now_ms();
	int pid, status = 0, how = PORT_MENU_CONTINUE, i;

	if (!menu_elf) return 0;
	/* The bar down and let go of, and the display found afresh */
	release(false);
	if (!master_copy()) {
		/* Still starting, under its loading screen: the menu over that */
		if (menu_waiting()) return 1;
		fprintf(stderr, "portkeys: MENU, but no program holds the display; no menu\n");
		return 0;
	}
	pid = G.pid;
	pg = getpgid(pid);
	if (pg <= 1 || pg == getpgrp()) {
		fprintf(stderr, "portkeys: MENU, but pid %d is not a port's; no menu\n", pid);
		release(false);
		return 0;
	}
	fprintf(stderr, "portkeys: MENU, the in-game menu over pid %d\n", pid);
	kill(-pg, SIGSTOP);
	for (i = 0; i < 50 && !stopped(pid); i++) usleep(2000);
	/* A refresh more, for a flip it had asked for to land: then the frame on
	 * the panel is the one it stopped on */
	usleep(20000);
	memset(&c, 0, sizeof c);
	c.crtc_id = G.crtc;
	if (ioctl(G.fd, DRM_IOCTL_MODE_GETCRTC, &c) < 0) c.fb_id = 0;
	unlink(PORT_MENU_STILL);
	if (c.fb_id) still_save(c.fb_id);
	conn = crtc_connector();
	if (ioctl(G.fd, DRM_IOCTL_DROP_MASTER, 0) < 0) {
		fprintf(stderr, "portkeys: cannot let go of the port's display (%s); no menu\n", strerror(errno));
		kill(-pg, SIGCONT);
		release(false);
		return 0;
	}

	/* The memory the menu starts into: a port such as Stardew Valley leaves
	 * little, and the menu, not the stopped game, is what the kernel takes
	 * if it runs out (the child's oom_score_adj, src/portmenu.h) */
	fprintf(stderr, "portkeys: %ld MB free for the menu\n", mem_available_mb());
	follow_hold(true);
	child = fork();
	if (child == 0) {
		int fd = open("/proc/self/oom_score_adj", O_WRONLY | O_CLOEXEC);

		if (fd >= 0) {
			if (write(fd, PORT_MENU_OOM, strlen(PORT_MENU_OOM)) < 0) {}
			close(fd);
		}
		execl(menu_elf, menu_elf, PORT_MENU_ARG, menu_tag, menu_game, extra, (char *)NULL);
		_exit(127);
	}
	if (child < 0) fprintf(stderr, "portkeys: cannot start the menu (%s)\n", strerror(errno));
	while (child > 0 && waitpid(child, &status, 0) < 0 && errno == EINTR) {}
	follow_hold(false);
	if (child > 0 && WIFEXITED(status) &&
	    (WEXITSTATUS(status) == PORT_MENU_QUIT || WEXITSTATUS(status) == PORT_MENU_POWER ||
	     WEXITSTATUS(status) == PORT_MENU_RESET || WEXITSTATUS(status) == PORT_MENU_RESTART))
		how = WEXITSTATUS(status);
	else if (child > 0 && !(WIFEXITED(status) && WEXITSTATUS(status) == PORT_MENU_CONTINUE))
		fprintf(stderr, "portkeys: the menu ended with status %d; the game goes on\n", status);

	/* The display back to the port, and its frame on the panel. The
	 * launcher's SDL puts back the frame it found as it closes its window;
	 * one that did not (a crash) left the panel dark or on its own picture */
	for (i = 0; i < 150 && ioctl(G.fd, DRM_IOCTL_SET_MASTER, 0) < 0; i++) usleep(2000);
	if (i == 150) {
		fprintf(stderr, "portkeys: cannot give the display back to the port (%s)\n", strerror(errno));
	} else if (c.fb_id && c.mode_valid && conn) {
		struct drm_mode_crtc now = { .crtc_id = G.crtc };

		if (ioctl(G.fd, DRM_IOCTL_MODE_GETCRTC, &now) < 0 || now.fb_id != c.fb_id || !now.mode_valid) {
			c.set_connectors_ptr = (uintptr_t)&conn;
			c.count_connectors = 1;
			if (ioctl(G.fd, DRM_IOCTL_MODE_SETCRTC, &c) < 0)
				fprintf(stderr, "portkeys: cannot put the port's frame back (%s)\n", strerror(errno));
		}
	}
	/* Nothing held as the game goes on: the A that chose Continue, the B or
	 * MENU that closed the menu. SDL asks the pad what is down once its
	 * queue is emptied, so a button still held would reach the game as a
	 * press, and gptokeyb would type its key */
	wait_released(1500);
	port_input_flush(pg);
	release(false);
	kill(-pg, SIGCONT);
	/* The launcher's volume and brightness keys, saved where these are, and
	 * the boosts it kept beside them, its gain written again (pkkeys.h) */
	pk_levels_load(db, BACKLIGHT, &L);
	pk_boost_load(db, &L, &BO);
	/* And the hotkeys, which the menu's Controls may have changed, with
	 * Auto Off's time (keys_load) */
	keys_load();
	fprintf(stderr, "portkeys: the menu was up %ld ms: %s\n", now_ms() - t0,
	        how == PORT_MENU_QUIT ? "Quit" : how == PORT_MENU_POWER ? "POWER" :
	        how == PORT_MENU_RESET ? "Reset" : how == PORT_MENU_RESTART ? "start again" : "Continue");
	/* Quit, or a start again (a Display mode over your PICO-8): launch.sh
	 * tells the two apart by the restart file the menu wrote (port_quit).
	 * The program going away is not borrowed from again (gone_pid) */
	if (how == PORT_MENU_RESTART) gone_pid = pid;
	if (how == PORT_MENU_QUIT || how == PORT_MENU_RESTART) {
		pid_t parent = getppid();

		if (parent > 1) kill(parent, SIGUSR2);
	} else if (how == PORT_MENU_POWER) {
		end_port();
	} else if (how == PORT_MENU_RESET) {
		/* PICO-8 going on, its queues emptied of the menu's presses: its
		 * own key for a restart of the cart (pkkbd.h) */
		if (pkkbd_ctrl_r(kbd, nap_ms)) fprintf(stderr, "portkeys: CTRL-R, the cart starts again\n");
		else fprintf(stderr, "portkeys: Reset, but no keyboard to send CTRL-R on\n");
	}
	return 1;
}

static int menu_show(void)
{
	return menu_show_as(NULL);
}

/* ---- your PICO-8's hotkeys -------------------------------------------------
 *
 * SELECT held and one more button, the player's own (src/hotkeys.h,
 * DESIGN.md 11.61), over your PICO-8 as over a Diatom game: the launcher
 * hands over the table in force for the cart (src/portmenu.h's
 * PORT_KEYS_FILE), the joypad's events go through Diatom's chord rule as
 * they are read (key_seen, tools/pkhot.c), and what they ask for is done
 * here after the pass (hot_do). PICO-8 sees SELECT and the button all the
 * same: nothing is grabbed.
 *
 *   Screenshot     PICO-8's own 128 x 128 picture, as Diatom writes fake08's,
 *                  to Screenshots/PICO-8/<the cart> <time>.png, and the
 *                  pop-up with the shot in it (shot_take)
 *   Reset Game     asked first, as over a game: PICO-8 stopped and the
 *                  launcher's question over its frame (menu_show_as with
 *                  PORT_MENU_RESET_ARG); Reset is CTRL-R (pkkbd.h)
 *   Quit           as the menu's Quit: USR2 to launch.sh
 *   Next Shader,   the system's rows stepped (shader_next, display_next)
 *   Next Display
 *
 * Quick Save, Quick Load, Faster and Slower are no acts here (PKHOT_LIVE). */
static pkhot HOT;
static unsigned hot_pending;     /* the acts this pass's events made */
static char shots[1024];         /* PORT_KEYS_FILE's shots: the folder and the cart's name */

/* Auto Off over your PICO-8 (src/idle.h, the launcher's own clock): MENU >
 * Auto Off's time with no key pressed on any device, the charger holding it,
 * and the program ends and the handheld powers off as for POWER (end_port).
 * A game resumes where it was after Diatom's Auto Off; PICO-8 has no state
 * to come back to, so play since the cart's last save is lost, as with
 * POWER. idle_c.seconds is 0 over every other program (keys_load). */
static idle_clock idle_c;
static bool idle_input, idle_charging, idle_fired;

static void keys_load(void)
{
	char line[1100], run[256] = "";
	FILE *f;

	pkhot_set(&HOT, NULL);
	idle_c.seconds = 0;
	shots[0] = '\0';
	if (!menu_tag || !port_hotkeys(menu_tag)) return;
	if ((f = fopen(PORT_KEYS_FILE, "r"))) {
		while (fgets(line, sizeof line, f)) {
			line[strcspn(line, "\n")] = '\0';
			if (!strncmp(line, "hotkeys\t", 8)) snprintf(run, sizeof run, "%.255s", line + 8);
			else if (!strncmp(line, "shots\t", 6)) snprintf(shots, sizeof shots, "%.1023s", line + 6);
		}
		fclose(f);
	}
	if (run[0] && !pkhot_set(&HOT, run))
		fprintf(stderr, "portkeys: hotkeys \"%s\" not read; none\n", run);
	/* The launcher's MENU > Auto Off: two minutes until the player sets it
	 * (src/main.c's auto_off_load) */
	idle_c.seconds = pk_db_get(db, "autooff", 120);
	fprintf(stderr, "portkeys: hotkeys %s, Auto Off %d s\n", HOT.on ? run : "none", idle_c.seconds);
}

/* ---- the screenshot ---- */

static void png_u32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

/* A chunk at out: its length, type, data and CRC; the new end */
static uint8_t *png_chunk(uint8_t *out, const char *type, const uint8_t *data, uint32_t n)
{
	png_u32(out, n);
	memcpy(out + 4, type, 4);
	if (n) memcpy(out + 8, data, n);
	png_u32(out + 8 + n, (uint32_t)crc32(0L, out + 4, 4 + n));
	return out + 12 + n;
}

/* `rgb`, w x h, as an 8-bit RGB PNG, every row filter none, as Diatom
 * writes its screenshots; malloc'd */
static uint8_t *png_rgb(const uint8_t *rgb, int w, int h, size_t *len)
{
	static const uint8_t SIG[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
	const size_t row = (size_t)w * 3 + 1, raw_n = row * (size_t)h;
	uint8_t *raw, *out, *o, ihdr[13];
	uLongf zn;
	int y;

	*len = 0;
	if (!(raw = malloc(raw_n))) return NULL;
	for (y = 0; y < h; y++) {
		raw[(size_t)y * row] = 0;
		memcpy(raw + (size_t)y * row + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
	}
	zn = compressBound((uLong)raw_n);
	if (!(out = malloc(8 + 25 + 12 + zn + 12))) {
		free(raw);
		return NULL;
	}
	memcpy(out, SIG, 8);
	png_u32(ihdr, (uint32_t)w);
	png_u32(ihdr + 4, (uint32_t)h);
	ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
	o = png_chunk(out + 8, "IHDR", ihdr, 13);
	if (compress2(o + 8, &zn, raw, (uLong)raw_n, 6) != Z_OK) {
		free(raw);
		free(out);
		return NULL;
	}
	free(raw);
	png_u32(o, (uint32_t)zn);
	memcpy(o + 4, "IDAT", 4);
	png_u32(o + 8 + zn, (uint32_t)crc32(0L, o + 4, (uInt)(4 + zn)));
	o += 12 + zn;
	o = png_chunk(o, "IEND", NULL, 0);
	*len = (size_t)(o - out);
	return out;
}

/* Every folder of `path` but its last part, made as needed */
static void make_dirs(const char *path)
{
	char d[1300];
	size_t i;

	snprintf(d, sizeof d, "%s", path);
	for (i = 1; d[i]; i++) {
		if (d[i] != '/') continue;
		d[i] = '\0';
		mkdir(d, 0755);
		d[i] = '/';
	}
}

/* PICO-8's 128 x 128 out of the frame on the panel: each of its pixels read
 * at its middle, in the rect it is drawn in for the Display mode in force
 * (src/p8draw.h, display.PICO8 read now: a restart can have changed it),
 * turned back to the landscape window PICO-8 drew on as the bar is turned
 * onto the panel (put). A middle off the panel (Zoomed, Edges Cut) is
 * black: that much of the picture was never on the screen. Sharp at any
 * factor: PICO-8's filtered modes are linear, and a pixel's middle is its
 * own color. */
static uint8_t g_shot[P8_SCREEN * P8_SCREEN * 3];

static void shot_pick(const frame_map *m)
{
	char key[64], mode[32];
	const bool turned = m->h > m->w;
	const int lw = (int)(turned ? m->h : m->w), lh = (int)(turned ? m->w : m->h);
	dmode_rect r;
	int i, j;

	snprintf(key, sizeof key, "display.%s", menu_tag);
	pkset_text(db, key, mode, sizeof mode);
	p8_draw_rect(mode[0] ? mode : NULL, lw, lh, P8_NEG_OK, &r);
	for (j = 0; j < P8_SCREEN; j++)
		for (i = 0; i < P8_SCREEN; i++) {
			int lx = r.x + (int)((i + 0.5) * r.w / P8_SCREEN), ly = r.y + (int)((j + 0.5) * r.h / P8_SCREEN);
			uint8_t *d = g_shot + ((size_t)j * P8_SCREEN + i) * 3;
			uint32_t v = 0;

			if (lx >= 0 && lx < lw && ly >= 0 && ly < lh) {
				int px = turned ? ly : lx, py = turned ? lw - 1 - lx : ly;

				memcpy(&v, m->px + (size_t)py * m->pitch + (size_t)px * 4, 4);
			}
			d[0] = (uint8_t)(v >> 16); d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)v;
		}
}

/* The pop-up, "Screenshot saved" and the folder, with the shot laid in the
 * square the launcher left for it (PORT_SHOT_LIGHT, NOTICE_SLOT): nearest
 * pixels, and the badge's own rounded corners (src/notice.c's draw_badge)
 * cut over the slab, at the theme the bar would be in now */
static struct {
	uint32_t *px[DS_THEMES], *show;
	int w[DS_THEMES], h[DS_THEMES], slot[DS_THEMES][4];
} SN;

static void shot_notice_load(void)
{
	SN.px[DS_LIGHT] = dtov_read(PORT_SHOT_LIGHT, &SN.w[DS_LIGHT], &SN.h[DS_LIGHT], SN.slot[DS_LIGHT]);
	SN.px[DS_DARK] = dtov_read(PORT_SHOT_DARK, &SN.w[DS_DARK], &SN.h[DS_DARK], SN.slot[DS_DARK]);
}

/* The pop-up on the cursor window, composed with the bar (until this build
 * has the window's composition: said) */
static void notice_show(const uint32_t *px, int w, int h, long ms)
{
	(void)px;
	fprintf(stderr, "portkeys: the pop-up, %d x %d, %ld ms\n", w, h, ms);
}

static void shot_popup(void)
{
	const int t = pkset_bar_theme(appearance_read ? appearance_word : NULL, time(NULL), THEME_FILE);
	const int *sl = SN.slot[t], side = sl[2], corner = sl[3];
	const int w = SN.w[t], h = SN.h[t];
	int x, y;

	if (!SN.px[t] || side <= 0 || sl[0] + side > w || sl[1] + side > h) {
		fprintf(stderr, "portkeys: no pop-up for the screenshot\n");
		return;
	}
	free(SN.show);
	if (!(SN.show = malloc((size_t)w * h * 4))) return;
	memcpy(SN.show, SN.px[t], (size_t)w * h * 4);
	for (y = 0; y < side; y++)
		for (x = 0; x < side; x++) {
			int dx = x < corner ? corner - x : (x >= side - corner ? x - (side - corner - 1) : 0);
			int dy = y < corner ? corner - y : (y >= side - corner ? y - (side - corner - 1) : 0);
			const uint8_t *c = g_shot + ((size_t)(y * P8_SCREEN / side) * P8_SCREEN + x * P8_SCREEN / side) * 3;
			uint32_t *d = SN.show + (size_t)(sl[1] + y) * w + sl[0] + x, under = *d, a = 255, k, o = 0;

			if (dx || dy) {
				double dd = sqrt((double)dx * dx + (double)dy * dy) - corner;

				if (dd > 0.5) continue;
				if (dd > -0.5) a = (uint32_t)(255 * (0.5 - dd));
			}
			/* Over the slab, premultiplied: the shot's color at `a` */
			for (k = 0; k < 32; k += 8) {
				uint32_t src = k == 24 ? 255 : c[2 - k / 8];

				o |= ((src * a + ((under >> k) & 255) * (255 - a) + 127) / 255) << k;
			}
			*d = o;
		}
	notice_show(SN.show, w, h, 1500);
}

static void shot_take(void)
{
	char when[32], path[1200], tmp[1210];
	struct drm_mode_crtc c;
	time_t now = time(NULL);
	struct tm tm;
	frame_map m;
	uint8_t *png;
	size_t len = 0;
	bool mine, ok;
	int fd;

	if (!shots[0]) {
		fprintf(stderr, "portkeys: screenshot: no folder handed over\n");
		return;
	}
	/* The display borrowed for the bar, or a master's copy for the read */
	mine = !held();
	if (mine && !master_copy()) {
		fprintf(stderr, "portkeys: screenshot: no program holds the display\n");
		return;
	}
	memset(&c, 0, sizeof c);
	c.crtc_id = G.crtc;
	ok = ioctl(G.fd, DRM_IOCTL_MODE_GETCRTC, &c) == 0 && c.fb_id && frame_open(c.fb_id, &m);
	if (ok) {
		shot_pick(&m);
		frame_close(&m);
	}
	if (mine) release(false);
	if (!ok) return;
	localtime_r(&now, &tm);
	strftime(when, sizeof when, "%Y-%m-%d %H.%M.%S", &tm);
	snprintf(path, sizeof path, "%s %s.png", shots, when);
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	make_dirs(path);
	png = png_rgb(g_shot, P8_SCREEN, P8_SCREEN, &len);
	fd = png ? open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644) : -1;
	ok = fd >= 0 && write_all(fd, png, len);
	if (fd >= 0 && close(fd)) ok = false;
	ok = ok && rename(tmp, path) == 0;
	if (!ok) unlink(tmp);
	free(png);
	fprintf(stderr, "portkeys: screenshot %s %s\n", ok ? "written" : "failed", path);
	if (ok) shot_popup();
}

/* Next Shader over your PICO-8: the shaders over it are drawn by a program
 * beside it that reads its settings from a file (src/p8fx.h). Until this
 * build has them, said and nothing done. */
static void shader_next(void)
{
	fprintf(stderr, "portkeys: Next Shader: no shaders over your PICO-8 in this build\n");
}

/* Next Display over your PICO-8: a new mode means PICO-8 starts again, so
 * PICO-8 is stopped as for the menu and the launcher steps the Display row
 * and asks first, as the menu's Display row does (PORT_MENU_DISPLAY_ARG):
 * Start Again is PORT_MENU_RESTART, the program started again in the new
 * mode (launch.sh), Cancel the game on where it was. 1 when it was up. */
static int display_next(void)
{
	fprintf(stderr, "portkeys: hotkey, Next Display: asked first\n");
	return menu_show_as(PORT_MENU_DISPLAY_ARG);
}

/* What this pass's hotkeys asked for, in Diatom's order (diatom/src/main.c):
 * the screenshot, the next shader and display, then Reset Game and Quit. 1
 * when a menu was up, and the keys here are then to be read again. */
static int hot_do(unsigned acts)
{
	int menu = 0;

	if (acts & (1u << CHORD_SHOT)) {
		fprintf(stderr, "portkeys: hotkey, Screenshot\n");
		shot_take();
	}
	if (acts & (1u << CHORD_SHADER)) shader_next();
	if (acts & (1u << CHORD_DISPLAY)) menu |= display_next();
	if (acts & (1u << CHORD_QUIT)) {
		pid_t parent = getppid();

		fprintf(stderr, "portkeys: hotkey, Quit\n");
		if (parent > 1) kill(parent, SIGUSR2);
	} else if (acts & (1u << CHORD_RESET)) {
		fprintf(stderr, "portkeys: hotkey, Reset Game: asked first\n");
		menu |= menu_show_as(PORT_MENU_RESET_ARG);
	}
	return menu;
}

/* Devices: the poll set's first n entries, and for each whether it has MENU
 * or the volume keys, whether the kernel's queue for it overflowed
 * (SYN_DROPPED) and what it sent since is still to be skipped, and whether
 * what is down on it must be queried after discarding this pass. */
static struct pollfd fds[9];
static bool has_menu[8], has_vol[8], dropped[8], resync[8];
static int n;
static pkkeys K;
/* A removed input invalidates the batch and its held state. */
static bool lost_menu, lost_vol;

/* Until no key is down on the joypad or the volume keys' device, as the
 * kernel says (EVIOCGKEY), `ms` at most. Marked unused for
 * tools/portkeys-flow-check.py, which builds these devices' code with a
 * menu_show of its own that never calls it. */
__attribute__((unused)) static void wait_released(int ms)
{
	long t0 = now_ms();

	for (;;) {
		bool held = false;
		int i;

		for (i = 0; i < n && !held; i++) {
			unsigned long down[NLONGS(KEY_MAX + 1)] = { 0 };
			size_t k;

			if (!(has_menu[i] || has_vol[i]) ||
			    ioctl(fds[i].fd, EVIOCGKEY(sizeof down), down) < 0)
				continue;
			for (k = 0; k < NLONGS(KEY_MAX + 1); k++)
				if (down[k]) held = true;
		}
		if (!held || now_ms() - t0 >= ms) {
			if (now_ms() - t0 > 20)
				fprintf(stderr, "portkeys: %s the buttons to be let go, %ld ms\n",
				        held ? "gave up waiting for" : "waited for", now_ms() - t0);
			return;
		}
		{
			struct timespec nap = { 0, 10 * 1000000L };

			nanosleep(&nap, NULL);
		}
	}
}

/* POWER over a port. ZyperOS reads the power key itself, and launch.sh stops
 * it for a port, so a press during Stardew Valley or OpenBOR did nothing. Its
 * device (the RK817's "rk805 pwrkey" on the Pixel 2) is read here on a file
 * of its own, apart from the keys above and their ordering: a press is the
 * power button's, whatever else is held, as it is in the launcher. The press
 * goes to launch.sh, which started this and is its parent, as SIGUSR1; it
 * ends the port, TERM and then KILL, and powers off as the launcher does
 * after its own power press. Never to init: a parent gone leaves this a child
 * of PID 1, where USR1 halts BusyBox's init. */
static int powfd = -1;

/* The port ends, and the handheld powers off: SIGUSR1 to launch.sh */
static void end_port(void)
{
	pid_t parent = getppid();

	if (parent > 1) kill(parent, SIGUSR1);
}

/* The battery run flat over a port, by the launcher's rule (battery_empty,
 * src/main.c): BATT_EMPTY_PCT or under, off the charger, on two readings in a
 * row 5 s apart, so one stray reading never powers off. It ends the port and
 * powers off as POWER does, while the card can still be let go of, where the
 * PMIC's own cut would leave it mounted. Auto Off does not run over a port:
 * the launcher takes it only because a game resumes where it was after the
 * power-off (AUTO_OFF, src/main.c), and a port has no resume. */
#define BATT_EVERY_MS 5000
static long batt_next;
static int batt_seen;

/* One reading: true on the second empty one in a row, and only then */
static bool batt_empty(int pct, bool charging)
{
	if (!charging && pct >= 0 && pct <= BATT_EMPTY_PCT) batt_seen++;
	else batt_seen = 0;
	return batt_seen == 2;
}

/* Auto Off's look, once a pass of the loop (idle_c, over your PICO-8):
 * whether a key came since the last (key_seen), the charger as the battery
 * was last read. The clock starts again after a gap of more than
 * IDLE_GAP_MS between two looks, as the launcher's does after a time nobody
 * read the pad: the loop wakes at least that often while it counts, so a
 * gap is the in-game menu, whose launcher has the keys meanwhile. Once,
 * then launch.sh ends this process. */
static void idle_tick(long now)
{
	if (idle_check(&idle_c, (unsigned)now, idle_input, idle_charging) && !idle_fired) {
		idle_fired = true;
		fprintf(stderr, "portkeys: no key for %d s (Auto Off); the port ends\n", idle_c.seconds);
		end_port();
	}
	idle_input = false;
}

/* A press, or with MENU > Power Button on Hold a hold of POWERHOLD_MS, as
 * the launcher reads it (src/powerhold.h): PW is the key's state, and the
 * bar shows the hold's power kind for as long as it is under way, full when
 * it is done, gone at once when it is let go early (pw_bar). */
static powerhold PW;
static bool pw_shown;

static void pw_bar(long now)
{
	int pm = powerhold_permille(&PW, (unsigned)now);

	if (pm >= 0) {
		bar_show(OSDBAR_POWER, pm * OSDBAR_POWER_STEPS / 1000, OSDBAR_POWER_STEPS);
		pw_shown = true;
	} else if (pw_shown) {
		pw_shown = false;
		if (PW.fired) bar_show(OSDBAR_POWER, OSDBAR_POWER_STEPS, OSDBAR_POWER_STEPS);
		else bar_fade();
	}
}

static void power_read(void)
{
	struct input_event ev[16];
	ssize_t r;

	while ((r = read(powfd, ev, sizeof ev)) > 0) {
		size_t k;

		for (k = 0; k < (size_t)r / sizeof *ev; k++) {
			/* A repeat is neither a press nor a let-go */
			if (ev[k].type != EV_KEY || ev[k].code != KEY_POWER || ev[k].value == 2)
				continue;
			if (!powerhold_key(&PW, ev[k].value == 1, (unsigned)now_ms())) continue;
			fprintf(stderr, "portkeys: POWER, the port ends\n");
			end_port();
		}
	}
	if (r == 0 || (r < 0 && errno != EAGAIN && errno != EINTR)) {
		close(powfd);
		powfd = -1;
		PW = (powerhold){ 0 };
	}
	pw_bar(now_ms());
}

/* Every pass: a hold of POWER done ends the port as a press does */
static void power_due(void)
{
	long now = now_ms();

	if (powerhold_due(&PW, (unsigned)now)) {
		fprintf(stderr, "portkeys: POWER held, the port ends\n");
		end_port();
	}
	pw_bar(now);
}

/* After the in-game menu (menu_show), which read POWER itself: what the power
 * key's file queued meanwhile was the launcher's, and is let go of unread */
static void power_drain(void)
{
	struct input_event ev[16];
	ssize_t r = 1;

	while (powfd >= 0 && (r = read(powfd, ev, sizeof ev)) > 0) {}
	if (powfd >= 0 && (r == 0 || (r < 0 && errno != EAGAIN && errno != EINTR))) {
		close(powfd);
		powfd = -1;
	}
	/* And a hold under way when the menu came up was the launcher's too */
	PW = (powerhold){ 0 };
	pw_bar(now_ms());
}

/* The clock the devices stamp their events with: CLOCK_MONOTONIC, set on
 * each at open (EVIOCSCLOCKID), so a change of the wall clock - the RTC read
 * late, a time set over SSH - cannot reorder two events of one pass. If a
 * device will not take it, all of them go by the wall clock, where the
 * kernel starts them: the same clock for every device, which is what the
 * order needs, and pending events mature on the next pass unless an earlier new
 * event defers their suffix. Queue overflow discards that entire batch. */
static clockid_t ev_clock = CLOCK_MONOTONIC;

static bool ev_clock_us(int64_t *us)
{
	struct timespec t;

	if (clock_gettime(ev_clock, &t) < 0) {
		fprintf(stderr, "portkeys: cannot read the event clock: %s\n", strerror(errno));
		return false;
	}
	*us = (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
	return true;
}

/* Fail closed if even one reset fails: that fd might still use monotonic
 * timestamps while another uses realtime. No pass may mix those clocks. */
static bool event_clock_fallback(int clk_err)
{
	int clk = CLOCK_REALTIME;

	if (!clk_err) return true;
	fprintf(stderr, "portkeys: a device has no monotonic clock (%s); trying the wall clock\n",
	        strerror(clk_err));
	for (int i = 0; i < n; i++) {
		if (ioctl(fds[i].fd, EVIOCSCLOCKID, &clk) < 0) {
			fprintf(stderr, "portkeys: cannot set a shared event clock: %s\n", strerror(errno));
			return false;
		}
	}
	ev_clock = CLOCK_REALTIME;
	return true;
}

/* A device that is gone - unplugged, or its driver unbound - reports
 * POLLHUP and POLLERR for ever and a read of it fails: polled on, it would
 * spin a core. Its place goes to the last one. */
static void drop_device(int i)
{
	fprintf(stderr, "portkeys: an input device went away\n");
	if (fds[i].fd == jack) jack = -1;
	/* Discard its pending events with this pass; it cannot release its keys. */
	lost_menu |= has_menu[i];
	lost_vol |= has_vol[i];
	if (has_menu[i]) pkhot_resync(&HOT, 0);
	close(fds[i].fd);
	n--;
	fds[i] = fds[n];
	has_menu[i] = has_menu[n];
	has_vol[i] = has_vol[n];
	dropped[i] = dropped[n];
	resync[i] = resync[n];
}

/* Hands what was read to pkkeys, and does the steps it gives */
static bool keys_pass(const pk_key *keys, int nk, int64_t cutoff)
{
	pk_step s[PK_WAITING + PK_PASS_MAX];
	int i, m = pkkeys_pass(&K, keys, nk, cutoff, now_ms(), s, (int)(sizeof s / sizeof s[0]));

	if (m < 0) return false;
	for (i = 0; i < m; i++) press(&s[i]);
	return true;
}

/* Every key event of every device, before pkkeys picks its own: somebody is
 * there (Auto Off's clock, idle_tick), and on the joypad, SELECT's hotkeys
 * over your PICO-8, judged in the order the kernel queued them (pkhot.h) and
 * done after the pass (hot_do). The joypad's state is followed with no
 * hotkeys too, so a table given after a menu starts from the pad as it is. */
static void key_seen(int i, const struct input_event *e)
{
	idle_input = true;
	if (has_menu[i])
		hot_pending |= pkhot_key(&HOT, e->code, e->value,
		                         (uint64_t)((int64_t)e->input_event_sec * 1000 + e->input_event_usec / 1000));
}

/* The joypad's buttons down, as pkhot counts them, from EVIOCGKEY's bits */
static uint32_t pad_held(const unsigned long *down)
{
	static const uint16_t codes[] = {
		PKHOT_BTN_SOUTH, PKHOT_BTN_EAST, PKHOT_BTN_NORTH, PKHOT_BTN_WEST, PKHOT_BTN_TL, PKHOT_BTN_TR,
		PKHOT_BTN_TL2, PKHOT_BTN_TR2, PKHOT_BTN_SELECT, PKHOT_BTN_START, PKHOT_BTN_UP,
		PKHOT_BTN_UP + 1, PKHOT_BTN_UP + 2, PKHOT_BTN_UP + 3, PK_BTN_MENU,
	};
	uint32_t held = 0;
	size_t k;

	for (k = 0; k < sizeof codes / sizeof codes[0]; k++)
		if (HAS(down, codes[k])) held |= pkhot_bit(codes[k]);
	return held;
}

/* Bound raw reads too: irrelevant events and EINTR must not keep this loop
 * inside one noisy device forever. Reaching the budget discards the batch,
 * even if that last read happened to empty the queue. */
#define PK_READ_CALLS 16

/* Read until EAGAIN, within the budget. Never apply steps here: all devices
 * must contribute before any step. False means EOF, malformed data or a
 * device error. `unreliable` requires discarding/resyncing the whole pass. */
static bool read_device(int i, pk_key *keys, int *nk, bool *unreliable, bool *plugged)
{
	struct input_event ev[64];
	ssize_t r;
	size_t k;

	for (int attempt = 0; attempt < PK_READ_CALLS; attempt++) {
		if ((r = read(fds[i].fd, ev, sizeof ev)) < 0) {
			if (errno == EINTR) continue;
			/* EAGAIN: all of it read. Anything else is a device gone, with
			 * events still queued perhaps (evdev_read says ENODEV) */
			return errno == EAGAIN;
		}
		if (!r || (size_t)r % sizeof ev[0]) return false;
		for (k = 0; k < (size_t)r / sizeof ev[0]; k++) {
			/* Lost events invalidate the whole pass. Skip the fragment
			 * through SYN_REPORT, then query state in resync_devices. */
			if (ev[k].type == EV_SYN && ev[k].code == SYN_DROPPED) {
				*unreliable = true;
				dropped[i] = true;
				continue;
			}
			if (dropped[i]) {
				if (ev[k].type == EV_SYN && ev[k].code == SYN_REPORT) {
					dropped[i] = false;
					resync[i] = true;
				}
				continue;
			}
			/* A plug in or out: the other output's window, as the
			 * launcher does (plat_audio_jack_poll) */
			if (ev[k].type == EV_SW && ev[k].code == SW_HEADPHONE_INSERT) {
				*plugged = true;
				continue;
			}
			if (ev[k].type == EV_KEY) key_seen(i, &ev[k]);
			if (ev[k].type != EV_KEY || !pkkeys_relevant(ev[k].code, ev[k].value)) continue;
			if (*nk == PK_PASS_MAX) {
				*unreliable = true;
				continue;
			}
			keys[(*nk)++] = (pk_key){ (int64_t)ev[k].input_event_sec * 1000000 + ev[k].input_event_usec,
			                          ev[k].code, ev[k].value };
		}
	}
	*unreliable = true;
	return true;
}

/* Lost ordering cancels every hold and pending event. Query every input,
 * since EVIOCGKEY also removes its queued key events. Recovered MENU may
 * select brightness for a later fresh press; a recovered volume hold never
 * synthesizes a press or repeat. A failed query removes that input. */
static void resync_devices(bool *plugged)
{
	pkkeys_init(&K);
	for (int i = 0; i < n; i++) {
		unsigned long down[NLONGS(KEY_MAX + 1)] = { 0 };

		if (dropped[i]) continue;  /* wait for SYN_REPORT before querying */
		resync[i] = false;
		if (has_menu[i] || has_vol[i]) {
			if (ioctl(fds[i].fd, EVIOCGKEY(sizeof down), down) < 0) {
				drop_device(i--);
				continue;
			}
			if (has_menu[i]) {
				pkkeys_menu_state(&K, K.menu_held || HAS(down, BTN_TRIGGER_HAPPY1));
				/* and SELECT's rule from what is down now (pkhot_resync) */
				pkhot_resync(&HOT, pad_held(down));
			}
		}
		if (fds[i].fd == jack) *plugged = true;
	}
	lost_menu = lost_vol = false;
}

int main(int argc, char **argv)
{
	int i, clk_err = 0;
	DIR *d = opendir("/dev/input");
	struct dirent *e;

	/* --follow PID first, from launch.sh for a program MP4 plays on over
	 * (follow_hold) */
	if (argc > 2 && !strcmp(argv[1], "--follow")) {
		follow_pid = (pid_t)atoi(argv[2]);
		argv += 2;
		argc -= 2;
	}
	/* --menu ELF TAG GAME next, from launch.sh for a port with the in-game
	 * menu (OpenBOR, a port, PortMaster): the launcher to show it, the system's tag and the
	 * game, for menu_show */
	if (argc > 4 && !strcmp(argv[1], "--menu")) {
		menu_elf = argv[2];
		menu_tag = argv[3];
		menu_game = argv[4];
		argv += 4;
		argc -= 4;
	}
	/* Over your PICO-8, its keyboard for CTRL-R first of all, so PICO-8's
	 * SDL finds it as it starts (SDL watches for one made later too) */
	if (menu_tag && port_resets(menu_tag)) kbd = pkkbd_open("/dev/uinput");
	osdbar_size(PANEL_S, &bw, &bh);
	pkkeys_init(&K);
	pkhot_init(&HOT);
	mixer = open("/dev/snd/controlC0", O_RDWR | O_CLOEXEC);
	/* The launcher's device database, from launch.sh: $USERDATA_PATH/zyperos.db */
	if (argc > 1 && sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
		fprintf(stderr, "portkeys: no settings at %s\n", argv[1]);
		sqlite3_close(db);
		db = NULL;
	}
	if (db) {
		sqlite3_busy_timeout(db, 200);
		/* As the launcher has it (src/db.c), for the same reason: a write
		 * on every step, a held key's repeats too, on the key path, and
		 * FULL costs 5x per write to keep the last nudge through a power
		 * cut. The journal mode is the file's and is WAL already; this one
		 * is the connection's, so it is said here too. */
		sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
	}
	/* As the launcher starts: the saved levels, or for the backlight the
	 * rung nearest what the panel is at. A volume never saved stays
	 * untouched until the first press, which starts from the middle. */
	pk_levels_load(db, BACKLIGHT, &L);
	/* And the boosts past the top kept beside them (pkkeys.h): the volume's
	 * gain written to the file the port's plugin reads, so the port starts
	 * with it; the backlight's lift below, with Night Mode's warmth */
	pk_boost_init(&BO, NULL);
	if (!pk_boost_load(db, &L, &BO))
		fprintf(stderr, "portkeys: no file at %s for the volume boost's gain\n", BO.share_path);
	if (pk_boost_cells(&BO, PK_VOLUME) || pk_boost_cells(&BO, PK_BRIGHT))
		fprintf(stderr, "portkeys: kept from before, volume boost +%d dB, brightness boost %d\n",
		        pk_boost_gain(&BO), pk_boost_lift(&BO));
	/* Developer SSH for the port's length (pkssh.h), and TERM to end it */
	pkssh_init(db, argc > 1 ? argv[1] : NULL);
	{
		struct sigaction sa;

		memset(&sa, 0, sizeof sa);
		sa.sa_handler = on_term;
		sigaction(SIGTERM, &sa, NULL);
	}
	/* The bar's theme (bar_show): MENU > Appearance, from the same device
	 * database the launcher reads it from, and the zone from the card's
	 * library, from launch.sh: $SHARED_USERDATA_PATH/.zyperos/library.db */
	appearance_read = pkset_text(db, "appearance", appearance_word, sizeof appearance_word);
	/* And the player's look and Auto's hours, which nothing changes while a
	 * port runs either: the bar's colors in both themes, and the hours the
	 * clock turns it at (src/look.h, src/theme.h) */
	if (appearance_read) look_load_db(db);
	/* MENU > Power Button, a press or a hold, as the launcher saved it
	 * (src/powerhold.h); nothing changes it while a port runs */
	{
		char pb[8];

		if (pkset_text(db, POWERHOLD_KEY, pb, sizeof pb)) powerhold_set_mode(!strcmp(pb, "hold"));
	}
	pkset_zone_load(argc > 2 ? argv[2] : NULL);
	/* Night Mode: the launcher left the table right as the port started */
	{
		panelgamma_color c;

		panelgamma_screen_read(PANELGAMMA_COLOR_FILE, &c, &night);
		night_on = panelgamma_night_at(&night, (long long)time(NULL));
		night_looked = time(NULL);
	}
	lift_start();
	/* The heat notice the launcher left, in both themes (heat_check) */
	heat_init(&H);
	notice_load(DS_LIGHT, PORT_HEAT_LIGHT);
	notice_load(DS_DARK, PORT_HEAT_DARK);
	/* Over your PICO-8: the hotkeys and Auto Off's time, and the
	 * screenshot's pop-up the launcher left (keys_load, shot_take) */
	keys_load();
	if (menu_tag && port_hotkeys(menu_tag)) shot_notice_load();

	/* Every event device with a volume key or MENU (BTN_TRIGGER_HAPPY1, on
	 * the joypad), and the headphone switch: found by what they report
	 * rather than by number, which follows probe order. The order found in
	 * no longer matters: the events are handled in the order of their times. */
	while (d && (e = readdir(d)) && n < 7) {
		char path[300];
		unsigned long keys[NLONGS(KEY_MAX)] = { 0 }, sw[NLONGS(SW_MAX + 1)] = { 0 };
		int fd, clk = CLOCK_MONOTONIC;

		if (strncmp(e->d_name, "event", 5)) continue;
		snprintf(path, sizeof path, "/dev/input/%s", e->d_name);
		/* Not blocking, so a pass reads each device to the end of its queue */
		if ((fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC)) < 0) continue;
		/* First, while nothing is queued yet: a change of clock empties a
		 * queue that has anything in it and says SYN_DROPPED, which is
		 * resynced like any other */
		if (ioctl(fd, EVIOCSCLOCKID, &clk) < 0 && !clk_err) clk_err = errno ? errno : EINVAL;
		ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys);
		ioctl(fd, EVIOCGBIT(EV_SW, sizeof sw), sw);
		/* The power key, on a file of its own (power_read) */
		if (powfd < 0 && HAS(keys, KEY_POWER))
			powfd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (HAS(keys, KEY_VOLUMEUP) || HAS(keys, BTN_TRIGGER_HAPPY1) ||
		    (jack < 0 && HAS(sw, SW_HEADPHONE_INSERT))) {
			if (jack < 0 && HAS(sw, SW_HEADPHONE_INSERT)) jack = fd;
			has_menu[n] = HAS(keys, BTN_TRIGGER_HAPPY1);
			has_vol[n] = HAS(keys, KEY_VOLUMEUP) || HAS(keys, KEY_VOLUMEDOWN);
			fds[n].fd = fd;
			fds[n++].events = POLLIN;
		} else {
			close(fd);
		}
	}
	if (d) closedir(d);
	if (!n) return 1;
	if (!event_clock_fallback(clk_err)) return 1;
	/* The plug as the port starts: a plug in or out from here on is a
	 * change of output (plug_changed) */
	jack_was = jack_in();

	for (;;) {
		static pk_key keys[PK_PASS_MAX];
		pk_step s[2];
		int wait = bar_tick(), rep = pkkeys_wait(&K, now_ms()), m, nk = 0;
		int pi = -1, pw = -1;
		bool plugged = false, unreliable = false;
		int64_t cutoff;

		if (g_term) {
			pkssh_stop();
			return 0;
		}
		if (!n && G.fd < 0) {
			fprintf(stderr, "portkeys: no keys left to read\n");
			pkssh_stop();
			return 0;
		}
		/* Developer SSH's next look (pkssh.h) */
		{
			int sw = pkssh_tick(now_ms());

			if (sw >= 0 && (wait < 0 || sw < wait)) wait = sw;
		}
		/* A held key's next repeat, or at once when events wait for this
		 * pass (pkkeys_wait) */
		if (rep >= 0 && (wait < 0 || rep < wait)) wait = rep;
		/* POWER held in Hold mode: the bar's next segment, or the hold's
		 * end, which no event marks */
		{
			int hw = powerhold_next_ms(&PW, (unsigned)now_ms(), OSDBAR_POWER_STEPS);

			if (hw >= 0 && (wait < 0 || hw < wait)) wait = hw;
		}
		/* While the port's file is borrowed, look at it at least five times a
		 * second. held() (kcmp, and whether this is still the master) is asked
		 * in bar_tick, which runs when this returns, and the wait above is up
		 * to the end of the 1.5 s the bar shows when nothing is pressed: a port
		 * that gave the master up or closed the display meanwhile would be
		 * found at the end of it and not before, all that time with its file
		 * and its master kept alive by this copy (see the top of this file). */
		if (G.fd >= 0 && (wait < 0 || wait > 200)) wait = 200;
		/* Night Mode's next start or end */
		{
			int nw = night_wait();

			if (nw >= 0 && (wait < 0 || nw < wait)) wait = nw;
		}
		/* The battery, every BATT_EVERY_MS (batt_empty) */
		{
			long now = now_ms();
			int pct;
			bool charging;

			if (now - batt_next >= 0) {
				if (batt_now(&pct, &charging)) {
					idle_charging = charging;
					if (batt_empty(pct, charging)) {
						fprintf(stderr, "portkeys: battery %d%%, empty; the port ends\n", pct);
						end_port();
					}
				}
				batt_next = now + BATT_EVERY_MS;
			}
			if (wait < 0 || wait > batt_next - now) wait = (int)(batt_next - now);
		}
		/* Auto Off counting: a look well inside its gap (idle_tick) */
		if (idle_c.seconds > 0 && (wait < 0 || wait > (int)IDLE_GAP_MS - 100)) wait = (int)IDLE_GAP_MS - 100;
		m = n;
		/* The port's end, while its display is borrowed */
		if (G.pidfd >= 0) {
			pi = m;
			fds[m].fd = G.pidfd;
			fds[m++].events = POLLIN;
		}
		/* And the power key */
		if (powfd >= 0) {
			pw = m;
			fds[m].fd = powfd;
			fds[m++].events = POLLIN;
		}
		if (poll(fds, (nfds_t)m, wait) < 0) continue;
		if (pi >= 0 && (fds[pi].revents & (POLLIN | POLLHUP | POLLERR))) release(true);
		if (pw >= 0 && fds[pw].revents) power_read();
		power_due();
		night_tick();

		/* poll is only a wakeup hint. A key may arrive on an fd after poll
		 * checked it but before another fd woke us. Read ALL open inputs,
		 * then handle a single ordered batch, within the stated bounds. */
		if (!ev_clock_us(&cutoff)) return 1;
		for (i = 0; i < n; i++) {
			if ((fds[i].revents & POLLNVAL) ||
			    !read_device(i, keys, &nk, &unreliable, &plugged) ||
			    (fds[i].revents & (POLLHUP | POLLERR))) {
				unreliable = true;
				drop_device(i--);
			}
		}
		for (i = 0; i < n; i++) unreliable |= dropped[i] || resync[i];
		unreliable |= lost_menu || lost_vol;
		if (unreliable || !keys_pass(keys, nk, cutoff)) resync_devices(&plugged);

		/* MENU on its own: the in-game menu, over a port that has one
		 * (menu_show). What was pressed while it was up was the launcher's,
		 * POWER and the volume keys too, so this side's keys start again
		 * from what is down now. */
		if (pkkeys_menu_tapped(&K) && menu_show()) {
			power_drain();
			resync_devices(&plugged);
		}
		/* SELECT's hotkeys over your PICO-8 (hot_do): Reset Game's question
		 * is a menu too, after which the keys start again in the same way */
		if (hot_pending) {
			const unsigned acts = hot_pending;

			hot_pending = 0;
			if (hot_do(acts)) {
				power_drain();
				resync_devices(&plugged);
			}
		}
		idle_tick(now_ms());

		if (plugged) plug_changed();

		/* A held key's repeats, on the launcher's clock */
		m = pkkeys_tick(&K, now_ms(), s, 2);
		for (i = 0; i < m; i++) press(&s[i]);
		/* And the boost's, after this pass's steps (boost_clock) */
		boost_clock(&K);
	}
}

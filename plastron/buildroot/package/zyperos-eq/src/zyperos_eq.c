/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * zyperos-eq: ZyperOS's equalizer as a LADSPA plugin, for ports.
 *
 * Every program here plays through ALSA's `default`, ports too, with no
 * sound server between. ZyperOS shapes the sound of the programs it is
 * (MP4, the menu music, Diatom) in their own code; a port's is shaped here.
 * launch.sh starts every port with an ALSA config of its own whose
 * `default` is plug -> alsa-lib's ladspa PCM (this plugin) -> plug -> the
 * same dmix every other program mixes into (the same ipc_key). Since the
 * volume boost (src/boost.h) it is in every port's chain, as the boost may
 * open during the port: flat and with no gain it copies the sound as it
 * came. The boost's gain is read from the same file as the bands, written
 * by portkeys while the port runs.
 *
 * The arithmetic is ZyperOS's src/eqdsp.h, the same file MP4, the menu
 * music and Diatom use (this package builds against the ZyperOS tree at
 * /zyperos, as the zyperos and diatom packages do): five bands, each gain
 * moving over 20 ms, flat touching nothing.
 *
 * The bands come from the launcher, which keeps those for the output in use
 * in /run/zyperos/eq (eqdsp.h's eq_share). The file is mapped once and read
 * at the start of every period, which is a few loads, no system call, so a
 * port follows the launcher's in-game menu, where the bands can change. A
 * file that is not there yet is looked for again about once a second.
 *
 * One plugin, stereo: two inputs and two outputs, the config binding the
 * port's two channels to them (policy none). Label "zyperos_eq".
 */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ladspa.h"
#include "eqdsp.h"

/* ZyperOS's own number; the config loads the plugin by its file and label,
 * never by this */
#define ZEQ_ID 4253

enum { IN_L, IN_R, OUT_L, OUT_R, PORTS };

typedef struct {
	LADSPA_Data *port[PORTS];
	eq_state     eq;
	unsigned long rate;
	const volatile eq_share *share;   /* the launcher's bands, mapped; NULL until there */
	unsigned long look_in;            /* frames until the file is looked for again */
	float        buf[EQ_SEG * 2];
} zeq;

static const char *share_path(void)
{
	const char *e = getenv("ZYPEROS_EQ_FILE");

	return e && *e ? e : EQ_SHARE_PATH;
}

static const volatile eq_share *share_map(void)
{
	int fd = open(share_path(), O_RDONLY | O_CLOEXEC);
	void *m;

	if (fd < 0) return NULL;
	m = mmap(NULL, sizeof(eq_share), PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	return m == MAP_FAILED ? NULL : (const volatile eq_share *)m;
}

/* The launcher's bands and the volume boost's gain (which portkeys writes
 * while the port runs), when there are some and they are whole: true with
 * them in `db` and `*gain`. Looked for about once a second while there is
 * no file. */
static int bands(zeq *z, unsigned long frames, int8_t db[EQ_BANDS], int8_t *gain)
{
	if (!z->share) {
		if (z->look_in > frames) {
			z->look_in -= frames;
			return 0;
		}
		z->look_in = z->rate;
		if (!(z->share = share_map())) return 0;
	}
	return eq_share_read(z->share, db, gain);
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long rate)
{
	zeq *z = calloc(1, sizeof *z);

	(void)d;
	if (!z) return NULL;
	z->rate = rate ? rate : 48000;
	eq_init(&z->eq, (int)z->rate);
	return z;
}

static void connect_port(LADSPA_Handle h, unsigned long port, LADSPA_Data *where)
{
	zeq *z = h;

	if (port < PORTS) z->port[port] = where;
}

/* A stream starting: at the bands as they are, with no move from flat */
static void activate(LADSPA_Handle h)
{
	zeq *z = h;
	int8_t db[EQ_BANDS] = { 0 }, gain = 0;

	z->look_in = 0;
	bands(z, 0, db, &gain);
	eq_init(&z->eq, (int)z->rate);
	eq_jump(&z->eq, db);
	eq_gain_jump(&z->eq, gain);
}

static void run(LADSPA_Handle h, unsigned long frames)
{
	zeq *z = h;
	const LADSPA_Data *l = z->port[IN_L], *r = z->port[IN_R];
	LADSPA_Data *ol = z->port[OUT_L], *or_ = z->port[OUT_R];
	int8_t db[EQ_BANDS], gain = 0;
	unsigned long at = 0, i;

	if (!l || !r || !ol || !or_) return;
	if (bands(z, frames, db, &gain)) {
		eq_set(&z->eq, db);
		eq_gain(&z->eq, gain);
	}
	if (!eq_active(&z->eq)) {
		/* Flat, no boost and at rest: the sound as it came */
		if (ol != l) memmove(ol, l, frames * sizeof *ol);
		if (or_ != r) memmove(or_, r, frames * sizeof *or_);
		return;
	}
	/* Through the shared arithmetic a run at a time: the two channels side
	 * by side, at 16-bit's scale, as eqdsp.h takes them */
	while (at < frames) {
		const unsigned long n = frames - at < EQ_SEG ? frames - at : EQ_SEG;

		for (i = 0; i < n; i++) {
			z->buf[i * 2] = l[at + i] * 32768.0f;
			z->buf[i * 2 + 1] = r[at + i] * 32768.0f;
		}
		eq_run_f(&z->eq, z->buf, (int)n);
		for (i = 0; i < n; i++) {
			ol[at + i] = z->buf[i * 2] * (1.0f / 32768.0f);
			or_[at + i] = z->buf[i * 2 + 1] * (1.0f / 32768.0f);
		}
		at += n;
	}
}

static void deactivate(LADSPA_Handle h)
{
	zeq *z = h;

	if (z->share) munmap((void *)z->share, sizeof(eq_share));
	z->share = NULL;
}

static void cleanup(LADSPA_Handle h)
{
	deactivate(h);
	free(h);
}

static const LADSPA_PortDescriptor PORT_KIND[PORTS] = {
	LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO, LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,
	LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO, LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
};
static const char *const PORT_NAME[PORTS] = { "Input L", "Input R", "Output L", "Output R" };
static const LADSPA_PortRangeHint PORT_HINT[PORTS] = { { 0, 0, 0 } };

static const LADSPA_Descriptor DESCRIPTOR = {
	.UniqueID = ZEQ_ID,
	.Label = "zyperos_eq",
	.Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
	.Name = "ZyperOS equalizer",
	.Maker = "ZyperOS",
	.Copyright = "GPL-3.0",
	.PortCount = PORTS,
	.PortDescriptors = PORT_KIND,
	.PortNames = PORT_NAME,
	.PortRangeHints = PORT_HINT,
	.instantiate = instantiate,
	.connect_port = connect_port,
	.activate = activate,
	.run = run,
	.deactivate = deactivate,
	.cleanup = cleanup,
};

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index)
{
	return index == 0 ? &DESCRIPTOR : NULL;
}

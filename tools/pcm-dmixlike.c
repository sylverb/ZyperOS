/* SPDX-License-Identifier: GPL-3.0-only */
/* A stand-in for the Pixel 2's dmix ("mixed" in plastron's
 * /etc/asound.conf), for a computer with no sound card: an alsa-lib external
 * PCM (ioplug) with dmix's limits there and nothing else - 48 kHz, two
 * channels of S16_LE, interleaved, 512-frame periods, a buffer of two to four
 * of them (2048 frames, its buffer_size). It takes hardware parameters and
 * plays nothing, which is all tools/eqchain-check.c asks of it: whether a
 * port's ALSA chain (sd/zyperos/eq-port.sh) settles its parameters over the
 * dmix as a program asks them. Loaded as `type dmixlike` with
 * pcm_type.dmixlike's `lib` naming this file. */
#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <stdlib.h>

static snd_pcm_sframes_t dl_pointer(snd_pcm_ioplug_t *io)
{
	return (snd_pcm_sframes_t)(io->hw_ptr % io->buffer_size);
}

static int dl_start(snd_pcm_ioplug_t *io) { (void)io; return 0; }
static int dl_stop(snd_pcm_ioplug_t *io) { (void)io; return 0; }

static int dl_close(snd_pcm_ioplug_t *io)
{
	free(io);
	return 0;
}

static const snd_pcm_ioplug_callback_t DL_CALLBACK = {
	.start = dl_start,
	.stop = dl_stop,
	.pointer = dl_pointer,
	.close = dl_close,
};

SND_PCM_PLUGIN_DEFINE_FUNC(dmixlike)
{
	static const unsigned int access[] = { SND_PCM_ACCESS_MMAP_INTERLEAVED, SND_PCM_ACCESS_RW_INTERLEAVED };
	static const unsigned int format[] = { SND_PCM_FORMAT_S16_LE };
	snd_pcm_ioplug_t *io;
	int r;

	(void)root;
	(void)conf;
	if (stream != SND_PCM_STREAM_PLAYBACK) return -EINVAL;
	if (!(io = calloc(1, sizeof *io))) return -ENOMEM;
	io->version = SND_PCM_IOPLUG_VERSION;
	io->name = "dmix stand-in";
	io->mmap_rw = 1;
	io->callback = &DL_CALLBACK;
	if ((r = snd_pcm_ioplug_create(io, name, stream, mode)) < 0) {
		free(io);
		return r;
	}
	if ((r = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_ACCESS, 2, access)) < 0 ||
	    (r = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_FORMAT, 1, format)) < 0 ||
	    (r = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_CHANNELS, 2, 2)) < 0 ||
	    (r = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_RATE, 48000, 48000)) < 0 ||
	    (r = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIOD_BYTES, 512 * 4, 512 * 4)) < 0 ||
	    (r = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_BUFFER_BYTES, 1024 * 4, 2048 * 4)) < 0 ||
	    (r = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIODS, 2, 4)) < 0) {
		snd_pcm_ioplug_delete(io);
		return r;
	}
	*pcmp = io->pcm;
	return 0;
}

SND_PCM_PLUGIN_SYMBOL(dmixlike);

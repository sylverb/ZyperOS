/* SPDX-License-Identifier: GPL-3.0-only */
/* See alsatry.h. */
#include "alsatry.h"

#include <stdio.h>

#ifdef __linux__
#include <alsa/asoundlib.h>

/* What your PICO-8 asks SDL for (pico8_64 0.2.7's codo_sound_init_plat:
 * SDL_OpenAudio at 22050 Hz, AUDIO_S16LSB, one channel, its config.txt's
 * mix_buffer_size frames a period, 1024 unless set, 2048 where its comment
 * says to try it for choppy sound) */
static const struct {
	snd_pcm_format_t format;
	unsigned channels, rate;
	snd_pcm_uframes_t samples;
} ASKS[] = {
	{ SND_PCM_FORMAT_S16_LE, 1, 22050, 1024 },
	{ SND_PCM_FORMAT_S16_LE, 1, 22050, 2048 },
	{ SND_PCM_FORMAT_S16_LE, 1, 22050, 512 },
};

/* One ask, the calls SDL 2.32's ALSA backend makes, in its order
 * (ALSA_OpenDevice, ALSA_set_buffer_size): opened not blocking, any
 * parameters, interleaved, the format, the channels or the device's own,
 * the rate near, then the period near the samples, two periods at least,
 * the first such, the hardware parameters; then the software ones, an
 * avail_min of a period and a start threshold of 1. 0, or ALSA's error,
 * with what failed in `step`. */
static int try_one(const char *dev, int k, const char **step)
{
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hw, *hw2;
	snd_pcm_sw_params_t *sw;
	snd_pcm_uframes_t period = ASKS[k].samples;
	unsigned rate = ASKS[k].rate, channels = ASKS[k].channels, periods = 2;
	int r;

	*step = "open";
	if ((r = snd_pcm_open(&pcm, dev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) return r;
	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_alloca(&hw2);
	snd_pcm_sw_params_alloca(&sw);
	*step = "any parameters";
	if ((r = snd_pcm_hw_params_any(pcm, hw)) < 0) goto out;
	*step = "interleaved access";
	if ((r = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) goto out;
	*step = "format";
	if ((r = snd_pcm_hw_params_set_format(pcm, hw, ASKS[k].format)) < 0) goto out;
	*step = "channels";
	if (snd_pcm_hw_params_set_channels(pcm, hw, channels) < 0 &&
	    (r = snd_pcm_hw_params_get_channels(hw, &channels)) < 0)
		goto out;
	*step = "rate";
	if ((r = snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, NULL)) < 0) goto out;
	snd_pcm_hw_params_copy(hw2, hw);
	*step = "period size";
	if ((r = snd_pcm_hw_params_set_period_size_near(pcm, hw2, &period, NULL)) < 0) goto out;
	*step = "two periods";
	if ((r = snd_pcm_hw_params_set_periods_min(pcm, hw2, &periods, NULL)) < 0) goto out;
	*step = "the first periods";
	if ((r = snd_pcm_hw_params_set_periods_first(pcm, hw2, &periods, NULL)) < 0) goto out;
	*step = "hardware parameters";
	if ((r = snd_pcm_hw_params(pcm, hw2)) < 0) goto out;
	*step = "software parameters";
	if ((r = snd_pcm_sw_params_current(pcm, sw)) < 0 ||
	    (r = snd_pcm_sw_params_set_avail_min(pcm, sw, period)) < 0 ||
	    (r = snd_pcm_sw_params_set_start_threshold(pcm, sw, 1)) < 0 ||
	    (r = snd_pcm_sw_params(pcm, sw)) < 0)
		goto out;
	r = 0;
out:
	snd_pcm_close(pcm);
	return r;
}

int alsa_try_main(int argc, char **argv)
{
	const char *dev = argc > 2 ? argv[2] : "default";
	size_t k;

	for (k = 0; k < sizeof ASKS / sizeof ASKS[0]; k++) {
		const char *step;
		int r = try_one(dev, (int)k, &step);

		if (r < 0) {
			fprintf(stderr, "alsa-try: %s, as PICO-8 asks it (%u ch, %u Hz, %lu frames): %s failed: %s\n",
			        dev, ASKS[k].channels, ASKS[k].rate, (unsigned long)ASKS[k].samples, step,
			        snd_strerror(r));
			return 1;
		}
	}
	fprintf(stderr, "alsa-try: %s opens as PICO-8 asks it, %zu ways\n", dev, k);
	return 0;
}

#else

int alsa_try_main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	fprintf(stderr, "alsa-try: no ALSA in this build\n");
	return 2;
}

#endif

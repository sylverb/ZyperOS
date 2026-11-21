/* SPDX-License-Identifier: GPL-3.0-only */
/* A port's sound opened as SDL 2.32's ALSA backend opens it (ALSA_OpenDevice
 * and ALSA_set_buffer_size, in their order: not blocking, any parameters,
 * interleaved, the format, the channels, the rate near, then the period near
 * the samples asked, two periods at least, the first such, the hardware
 * parameters; then the software ones), at the rates and channels programs
 * ask: your PICO-8's 22050 Hz mono (pico8_64 0.2.7), stereo, 44100, 48000,
 * 32000, 11025. Through whatever "default" the environment's ALSA config
 * makes it (ALSA_CONFIG_PATH: a port's, sd/zyperos/eq-port.sh's).
 *
 *     eqchain-try LABEL
 *
 * A line for each, and exit 1 when one did not open. For
 * tools/eqchain-check.sh (check-eqplugin on Linux) and
 * tools/eqplugin-alsa.sh (the handheld's own alsa-lib). */
#include <alsa/asoundlib.h>
#include <stdio.h>

static const struct { unsigned rate, channels; snd_pcm_uframes_t samples; } ASKS[] = {
	{ 22050, 1, 1024 }, { 22050, 1, 512 }, { 22050, 2, 1024 }, { 22050, 2, 512 },
	{ 44100, 2, 1024 }, { 44100, 1, 1024 }, { 48000, 2, 1024 }, { 48000, 2, 512 },
	{ 32000, 2, 1024 }, { 11025, 1, 1024 }, { 11025, 1, 512 },
};

static int try_one(int k, const char **step)
{
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hw, *hw2;
	snd_pcm_sw_params_t *sw;
	snd_pcm_uframes_t period = ASKS[k].samples;
	unsigned rate = ASKS[k].rate, channels = ASKS[k].channels, periods = 2;
	int r;

	*step = "open";
	if ((r = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) return r;
	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_alloca(&hw2);
	snd_pcm_sw_params_alloca(&sw);
	*step = "any parameters";
	if ((r = snd_pcm_hw_params_any(pcm, hw)) < 0) goto out;
	*step = "interleaved access";
	if ((r = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) goto out;
	*step = "format";
	if ((r = snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE)) < 0) goto out;
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

int main(int argc, char **argv)
{
	const char *label = argc > 1 ? argv[1] : "default";
	size_t k;
	int fails = 0;

	for (k = 0; k < sizeof ASKS / sizeof ASKS[0]; k++) {
		const char *step;
		int r = try_one((int)k, &step);

		printf("  %s %s: %u Hz, %u ch, a period of %lu", r < 0 ? "FAIL" : "PASS", label, ASKS[k].rate,
		       ASKS[k].channels, (unsigned long)ASKS[k].samples);
		if (r < 0) printf(": %s failed, %s", step, snd_strerror(r));
		printf("\n");
		if (r < 0) fails++;
	}
	return fails ? 1 : 0;
}

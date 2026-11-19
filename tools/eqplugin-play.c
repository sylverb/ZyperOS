/* SPDX-License-Identifier: GPL-3.0-only */
/* A port's sound, as the simplest port plays it: ALSA's "default", 48 kHz
 * S16 stereo, a second of a 1 kHz tone at a quarter of full scale. Then the
 * level of what reached the file the config writes (argv[1]), in dB against
 * the tone, and the check `argv[2]` names: "flat" within 0.05 dB of it,
 * "voice" about 3 dB over it. For tools/eqplugin-alsa.sh, run on the
 * handheld's own alsa-lib. */
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define RATE 48000

int main(int argc, char **argv)
{
	static short buf[RATE * 2];
	snd_pcm_t *pcm;
	FILE *f;
	double sq = 0;
	long n = 0;
	int i, r;

	if (argc < 3) return 2;
	remove(argv[1]);
	for (i = 0; i < RATE; i++)
		buf[i * 2] = buf[i * 2 + 1] = (short)lrint(0.25 * 32767.0 * sin(2.0 * M_PI * 1000.0 * i / RATE));
	if ((r = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0 ||
	    (r = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, RATE, 1, 100000)) < 0) {
		printf("  FAIL open default: %s\n", snd_strerror(r));
		return 1;
	}
	for (i = 0; i < RATE; ) {
		snd_pcm_sframes_t w = snd_pcm_writei(pcm, buf + i * 2, (snd_pcm_uframes_t)(RATE - i));

		if (w < 0) w = snd_pcm_recover(pcm, (int)w, 0);
		if (w < 0) break;
		i += (int)w;
	}
	snd_pcm_drain(pcm);
	snd_pcm_close(pcm);
	if (!(f = fopen(argv[1], "rb"))) {
		printf("  FAIL nothing reached %s\n", argv[1]);
		return 1;
	}
	/* The WAV the file plugin wrote, in the format the chain settled on, its
	 * first channel: 16-bit integers, or 32-bit floats, which is what the
	 * ladspa PCM hands its slave (the plug in front of the dmix makes them
	 * 16-bit on the handheld). alsa-lib's file plugin writes the floats'
	 * header with the integer tag, 1, so 32 bits are read as floats. */
	{
		unsigned char h[44];
		int ch, bits, bytes;

		if (fread(h, 1, 44, f) != 44 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
			printf("  FAIL %s is no WAV\n", argv[1]);
			return 1;
		}
		ch = h[22] | h[23] << 8;
		bits = h[34] | h[35] << 8;
		bytes = bits / 8;
		for (;;) {
			unsigned char fr[32];
			double v;

			if (ch < 1 || ch > 8 || bytes < 2 || bytes > 4 || fread(fr, (size_t)bytes, (size_t)ch, f) != (size_t)ch)
				break;
			if (bytes == 4) {
				float x;

				memcpy(&x, fr, 4);
				v = x * 32768.0;
			} else {
				v = (short)(fr[0] | fr[1] << 8);
			}
			if (n++ >= RATE / 2) sq += v * v;
		}
		printf("  (the chain wrote %d channels of %d-bit %s)\n", ch, bits, bits == 32 ? "floats" : "integers");
	}
	fclose(f);
	{
		const double db = 20.0 * log10(sqrt(sq / (n - RATE / 2)) / (0.25 * 32767.0 / sqrt(2.0)));
		const int ok = !strcmp(argv[2], "flat") ? fabs(db) < 0.05 : db > 2.4 && db < 3.6;

		printf("  %s %s through the ladspa PCM: %ld frames, %.3f dB at 1 kHz\n", ok ? "PASS" : "FAIL",
		       argv[2], n, db);
		return ok ? 0 : 1;
	}
}

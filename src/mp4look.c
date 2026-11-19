/* SPDX-License-Identifier: GPL-3.0-only */
/* See mp4look.h. */
#include "mp4look.h"

#include "design.h"

#include <math.h>
#include <string.h>

static const struct { const char *key; int bars; } LOOKS[MP4L_COUNT] = {
	[MP4L_COVER]    = { "cover",    0 },
	/* A Winamp's window of bars, under the clock: as many as fit the
	 * panel on the grid, 16 wide with 8 between (DESIGN.md 11.64) */
	[MP4L_CLASSIC]  = { "classic",  18 },
	/* One bar a band */
	[MP4L_SPECTRUM] = { "spectrum", 32 },
};

const char *mp4l_key(mp4l_id id)
{
	return id >= 0 && id < MP4L_COUNT ? LOOKS[id].key : LOOKS[MP4L_COVER].key;
}

mp4l_id mp4l_from_key(const char *key)
{
	int i;

	for (i = 0; key && i < MP4L_COUNT; i++)
		if (!strcmp(key, LOOKS[i].key)) return (mp4l_id)i;
	return MP4L_COVER;
}

int mp4l_bars(mp4l_id id)
{
	return id >= 0 && id < MP4L_COUNT ? LOOKS[id].bars : 0;
}

/* ---- the bars ---------------------------------------------------------------- */

/* Gravity, in heights a ms squared: from the top to the floor in
 * MP4L_CAP_DROP_MS, h = g t^2 / 2 */
#define CAP_G (2.0f / ((float)MP4L_CAP_DROP_MS * (float)MP4L_CAP_DROP_MS))
/* Under this a bar is at rest: less than a hundredth of a unit of its
 * height on the tallest bars drawn */
#define REST 0.002f

void mp4l_bars_reset(mp4l_bars_t *b, int n)
{
	memset(b, 0, sizeof *b);
	b->n = n < 0 ? 0 : n > MP4L_BARS_MAX ? MP4L_BARS_MAX : n;
}

/* Bar `i` of `n` over the 32 bands: the loudest of the bands under it, the
 * bands shared out in order, a bar taking one or two where n is not 32 */
static float target(const uint8_t *bands, int i, int n)
{
	int k0 = i * MP4L_BANDS / n, k1 = (i + 1) * MP4L_BANDS / n, k, top = 0;

	if (!bands) return 0.0f;
	if (k1 <= k0) k1 = k0 + 1;
	for (k = k0; k < k1 && k < MP4L_BANDS; k++)
		if (bands[k] > top) top = bands[k];
	return (float)top / 255.0f;
}

void mp4l_bars_step(mp4l_bars_t *b, float dt, const uint8_t *bands)
{
	/* follow's step for this frame, the bar's whole fall in one number */
	float fall = dt > 0 ? 1.0f - expf(-dt / (float)DS_MS_BAR_FALL) : 0.0f;
	int i;

	if (dt < 0) dt = 0;
	for (i = 0; i < b->n; i++) {
		float t = target(bands, i, b->n);

		/* Up at once; down on follow */
		if (t >= b->lv[i]) b->lv[i] = t;
		else b->lv[i] += (t - b->lv[i]) * fall;
		if (b->lv[i] < REST) b->lv[i] = 0.0f;

		/* The cap: on the bar wherever the bar reaches it, held there
		 * for peak-hold, then falling faster and faster until it is back
		 * on the bar */
		if (b->lv[i] >= b->cap[i]) {
			b->cap[i] = b->lv[i];
			b->hold[i] = (float)DS_MS_PEAK_HOLD;
			b->fall[i] = 0.0f;
		} else {
			float left = dt;

			if (b->hold[i] > 0) {
				float h = b->hold[i] < left ? b->hold[i] : left;

				b->hold[i] -= h;
				left -= h;
			}
			if (left > 0) {
				/* Exact for a fall that starts inside the frame */
				b->cap[i] -= b->fall[i] * left + 0.5f * CAP_G * left * left;
				b->fall[i] += CAP_G * left;
				if (b->cap[i] <= b->lv[i]) {
					b->cap[i] = b->lv[i];
					b->fall[i] = 0.0f;
				}
			}
		}
		if (b->cap[i] < REST) b->cap[i] = 0.0f;
	}
}

bool mp4l_bars_rest(const mp4l_bars_t *b)
{
	int i;

	for (i = 0; i < b->n; i++)
		if (b->lv[i] > 0.0f || b->cap[i] > 0.0f) return false;
	return true;
}

/* The frame loop's rule for a late turn at 1x, and how long a push waits
 * for room: see pace.h. */
#include "pace.h"

#include <math.h>

double pace_room_us(size_t queued, size_t n, int capacity, double rate)
{
	double over = (double)queued + (double)n - (double)capacity;

	if (capacity <= 0 || rate <= 0 || over <= 0) return 0;
	return over * 1e6 / rate;
}

double pace_lock_ratio(double refresh_hz, double core_fps)
{
	double r;

	if (refresh_hz <= 0 || core_fps <= 0) return 0;
	r = refresh_hz / core_fps;
	return r >= 1 - PACE_LOCK_MAX && r <= 1 + PACE_LOCK_MAX ? r : 0;
}

double pace_phase_step(double start_us, double vblank_us, double period_us,
                       double target_us, double gain, double max_us)
{
	double phase, err, step;

	if (period_us <= 0) return 0;
	phase = fmod(start_us - vblank_us, period_us);
	if (phase < 0) phase += period_us;
	err = phase - target_us;
	if (err > period_us / 2) err -= period_us;
	if (err <= -period_us / 2) err += period_us;
	step = -err * gain;
	if (step > max_us) step = max_us;
	if (step < -max_us) step = -max_us;
	return step;
}

pace_late pace_when_late(double late_us, double frame_us, double depth_us,
                         double *keep_us)
{
	*keep_us = late_us;
	if (late_us > frame_us * 4.0) {
		*keep_us = 0.0;
		return PACE_RESYNC;
	}
	if (depth_us <= 0.0 || late_us <= depth_us) return PACE_KEEP;
	*keep_us = depth_us;
	return PACE_CAUGHT_UP;
}

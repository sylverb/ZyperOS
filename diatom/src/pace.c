/* The frame loop's rule for a late turn at 1x, and how long a push waits
 * for room: see pace.h. */
#include "pace.h"

double pace_room_us(size_t queued, size_t n, int capacity, double rate)
{
	double over = (double)queued + (double)n - (double)capacity;

	if (capacity <= 0 || rate <= 0 || over <= 0) return 0;
	return over * 1e6 / rate;
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

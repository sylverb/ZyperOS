/* The frame loop's rule for a late turn at 1x: see pace.h. */
#include "pace.h"

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

/* What the frame loop does at 1x with a turn that ends at or past its
 * deadline (main.c, the pacing at the end of a turn).
 *
 * A little behind, the debt is kept, and the next turns run back to back
 * until it is repaid: a frame that ran long is made up, and the sound queue,
 * which the device kept playing from, fills back up. More than four frames
 * behind, something stalled, and catching up would only run the game fast
 * for a while: the debt is dropped (a resync).
 *
 * Between the two, part of the debt can be time the device already spent in
 * silence. A frame of 70 ms empties the queue, 43 ms deep at its target, and
 * the device pads the rest with silence; the loop is then 53 ms behind, under
 * the four frames, and repaid all of it. The sound of the repaid frames came
 * faster than it plays, on top of the silence already heard, and the queue
 * passed its capacity: the newest sound was refused. A gap, then a cut, a
 * tenth of a second apart. Measured with the stub core on the Mac's own sound
 * device (2026-11-16): one 70 to 75 ms frame dropped 403 to 1,105 frames, one
 * of 50 to 65 ms about 50 to 85, one of 80 ms or more none (it resyncs). The
 * Nintendo DS openings on the Pixel 2 dropped 500 to 1,489 the same way, in
 * the slow frames of their loading with light title frames after them.
 *
 * So a debt is kept only as deep as the queue. Every microsecond behind is a
 * microsecond of sound the queue gave the device and was not given back,
 * since each frame brings its own frame's time of sound, so a debt deeper
 * than the queue is time the device played silence for; that rest is dropped
 * as a resync's is. The depth is less than the target: SDL takes a device
 * period at a time (1,024 frames on the Pixel 2 and the Mac, a quarter of the
 * capacity) and pads with silence whatever the queue does not hold then, so
 * the queue's sound lasts down to a period, not to empty. Half the target:
 * 21 ms. Judged by the queue as read instead, a reading a thousand frames
 * either way of the level (131 to 2,912 around 2,048 on the Pixel 2, Contra,
 * 2026-10-01), a game a little slow all the time had its small debts dropped
 * half the time, and ran 1.5% slow with its queue run dry (the Mac,
 * 2026-11-16). */
#ifndef DIATOM_PACE_H
#define DIATOM_PACE_H

#include <stddef.h>

typedef enum {
	PACE_KEEP,       /* keep the whole debt; the next turns repay it */
	PACE_RESYNC,     /* more than four frames behind: drop it all */
	PACE_CAUGHT_UP,  /* keep the queue's depth of it, drop the rest */
} pace_late;

/* The queue's depth in frames, for a queue of `capacity`: half its target */
static inline double pace_depth_frames(int capacity) { return capacity / 4.0; }

/* How long until `n` more frames fit in a queue of `capacity` that holds
 * `queued`, at `rate` frames a second taken by the device: 0 when they fit
 * now. audio.c's HOLD waits this long for room (in steps). */
double pace_room_us(size_t queued, size_t n, int capacity, double rate);

/* `late_us`, how far past its deadline the turn ended (0 or more), at 1x;
 * `depth_us`, the queue's depth as time (0 when there is no sound device:
 * nothing to judge by, and the debt is kept as it always was). `*keep_us`
 * gets the debt to keep: all of `late_us`, none, or `depth_us`. */
pace_late pace_when_late(double late_us, double frame_us, double depth_us,
                         double *keep_us);

/* DISPLAY LOCK: a core whose rate is within PACE_LOCK_MAX of the panel's
 * runs at the panel's, one frame a refresh, as RetroArch's vsync runs it,
 * its sound resampled to match. The GKD Pixel 2's panel is 60.08 Hz (25 MHz
 * over 570 x 730) and a GBA 59.7275: paced at the core's own rate, its frames
 * drifted a tenth of a millisecond a refresh against the panel, and near the
 * boundary a millisecond's scheduling noise put them alternately before and
 * after it, a refresh shown twice and the next frame replaced unseen. Metroid
 * Fusion showed 58.2 to 58.9 new frames a second of the 59.73 it made, in
 * bursts every 2.9 s (2026-11-16): "mGBA has timing issues", a player said,
 * beside TwigUI's RetroArch. Locked, it runs 0.59% fast and even. */
#define PACE_LOCK_MAX 0.01

/* The panel's rate over the core's, when within PACE_LOCK_MAX of 1 (the game
 * runs that much faster, or slower, locked); 0 when not: no refresh known, no
 * core rate, or too far apart (a PAL game, 50 Hz) */
double pace_lock_ratio(double refresh_hz, double core_fps);

/* How far to move the next turn's start so turns start `target_us` after a
 * refresh: `start_us` the turn's start, `vblank_us` any refresh's time,
 * `period_us` the refresh period. A part `gain` of the error, at most
 * `max_us` either way, the error taken the short way round the period. */
double pace_phase_step(double start_us, double vblank_us, double period_us,
                       double target_us, double gain, double max_us);

#endif

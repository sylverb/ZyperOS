/* How fast a game is emulated, and how often it draws (emumeter.h). */
#include "emumeter.h"

double emumeter_frames_of(long runs, uint64_t sound_frames, double fps, double rate)
{
	double from_sound;

	if (runs <= 0) return 0.0;
	if (fps <= 0.0 || rate <= 0.0 || sound_frames == 0) return (double)runs;
	from_sound = (double)sound_frames * fps / rate;
	/* At most the calls', or a little over: the sound's pieces, not frames */
	if (from_sound <= (double)runs * EMUMETER_SOUND_SNAP) return (double)runs;
	return from_sound;
}

void emumeter_begin(emumeter *m, double fps, double sound_rate,
                    long runs, uint64_t sound, uint64_t pics, uint64_t run_us)
{
	m->fps = fps;
	m->rate = sound_rate;
	m->t0 = run_us;
	m->runs0 = runs;
	m->sound0 = sound;
	m->pics0 = pics;
	m->emu = (double)runs;
	m->per_run = 1.0;
	m->speed = -1.0;
	m->pic_fps = 0.0;
	m->have = 0;
}

int emumeter_turn(emumeter *m, long runs, uint64_t sound, uint64_t pics, uint64_t run_us)
{
	uint64_t dt;
	long n;
	double emu;

	/* A window of the game's own time; a clock that has not moved, or that
	 * went back (a new game), is not one */
	if (run_us < m->t0 + EMUMETER_WINDOW_US) return 0;
	dt = run_us - m->t0;
	n = runs - m->runs0;
	if (n > 0) {
		emu = emumeter_frames_of(n, sound - m->sound0, m->fps, m->rate);
		m->per_run = emu / (double)n;
	} else {
		emu = 0.0;
	}
	m->emu += emu;
	m->speed = m->fps > 0.0 ? emu * 1e6 / (double)dt / m->fps : -1.0;
	m->pic_fps = (double)(pics - m->pics0) * 1e6 / (double)dt;
	m->have = 1;
	m->t0 = run_us;
	m->runs0 = runs;
	m->sound0 = sound;
	m->pics0 = pics;
	return 1;
}

long emumeter_frames(const emumeter *m, long runs)
{
	return (long)(m->emu + (double)(runs - m->runs0) * m->per_run);
}

void emumeter_rebase(emumeter *m, long runs, uint64_t sound, uint64_t pics, uint64_t run_us)
{
	m->emu += (double)(runs - m->runs0) * m->per_run;
	m->t0 = run_us;
	m->runs0 = runs;
	m->sound0 = sound;
	m->pics0 = pics;
}

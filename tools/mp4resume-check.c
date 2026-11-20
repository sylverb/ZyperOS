/* SPDX-License-Identifier: GPL-3.0-only */
/* MP4's resume after a power-off (src/mp4resume.h): the row written and read
 * back, a broken one refused, and when the player's state asks for a write -
 * a session played through, second by second, counting the card writes. */
#include <stdio.h>
#include <string.h>

#include "../src/mp4resume.h"

static int fails, checks;

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static void row(void)
{
	mp4resume r = { .playing = true, .queue = MR_ALBUM, .at = 83.44 }, b;
	char s[700];

	printf("the row:\n");
	snprintf(r.track, sizeof r.track, "Music/Demo Library/First Light/01 First Light.flac");
	ck(mp4resume_format(&r, s, sizeof s) &&
	   !strcmp(s, "playing\talbum\t83.4\tMusic/Demo Library/First Light/01 First Light.flac"),
	   "written as playing, the queue, the second to a tenth, the file");
	ck(mp4resume_parse(s, &b) && b.playing && b.queue == MR_ALBUM && b.at > 83.39 && b.at < 83.41 &&
	   !strcmp(b.track, r.track), "and read back the same");
	r.playing = false;
	r.queue = MR_ALL;
	r.at = 0;
	ck(mp4resume_format(&r, s, sizeof s) && mp4resume_parse(s, &b) && !b.playing && b.queue == MR_ALL &&
	   b.at == 0, "paused, Shuffle All's, at its start");
	snprintf(r.track, sizeof r.track, "Videos/Trip\twith a tab.mp4");
	ck(!mp4resume_format(&r, s, sizeof s), "a file whose name holds a tab is not written");
	snprintf(r.track, sizeof r.track, "Audiobooks/A Long Name/Part 1.m4b");
	ck(!mp4resume_format(&r, s, 20), "nor one that does not fit");
	ck(!mp4resume_parse("", &b), "an empty row is none");
	ck(!mp4resume_parse("playing\talbum\t83.4", &b), "nor one with no file");
	ck(!mp4resume_parse("playing\talbum\t83.4\t", &b), "nor an empty file");
	ck(!mp4resume_parse("stopped\talbum\t83.4\tMusic/a.mp3", &b), "nor a state it does not know");
	ck(!mp4resume_parse("playing\tplaylist\t83.4\tMusic/a.mp3", &b), "nor a queue it does not know");
	ck(!mp4resume_parse("playing\talbum\tsoon\tMusic/a.mp3", &b), "nor a second that is no number");
	ck(!mp4resume_parse("playing\talbum\t-4\tMusic/a.mp3", &b), "nor one less than 0");
	ck(!mp4resume_parse("playing\talbum\tnan\tMusic/a.mp3", &b), "nor not a number at all");
	ck(mp4resume_parse("paused\talbum\t12.0\tMusic/x\ty.mp3", &b) && !strcmp(b.track, "Music/x\ty.mp3"),
	   "the file is the rest of the row");
}

/* A session, a step a second: how many writes, and what the last one says */
static void session(void)
{
	mp4resume_seen seen = { 0 };
	const char *a = "Music/A/01.mp3", *b = "Music/A/02.mp3";
	int t, writes = 0;
	double at = 0;

	printf("a session, second by second:\n");
#define STEP(heard, playing, track, now)                                                     \
	do {                                                                                     \
		if (mp4resume_due(&seen, heard, playing, track, at, now)) {                          \
			mp4resume_wrote(&seen, playing, track, at);                                      \
			writes++;                                                                        \
		}                                                                                    \
	} while (0)

	STEP(false, false, NULL, false);
	ck(writes == 0, "nothing loaded: nothing written");
	STEP(true, true, a, false);
	ck(writes == 1, "a queue starting: written at once");
	for (t = 1; t <= 95; t++) { at = t; STEP(true, true, a, false); }
	ck(writes == 4, "95 s of playing: once more every 30 s, 3 more");
	ck(seen.at == 90, "the last at 90 s");
	at = 95.5;
	STEP(true, false, a, false);
	ck(writes == 5 && !seen.playing && seen.at == 95.5, "paused: written at once, at its second");
	for (t = 0; t < 600; t++) STEP(true, false, a, false);
	ck(writes == 5, "ten minutes paused: nothing more");
	STEP(true, true, a, false);
	ck(writes == 6 && seen.playing, "played again: written");
	at = 0;
	STEP(true, true, b, false);
	ck(writes == 7 && !strcmp(seen.track, b), "the next song: written");
	at = 12.3;
	STEP(true, true, b, true);
	ck(writes == 8 && seen.at == 12.3, "the launcher powering off: written whatever changed");
	at = 13;
	STEP(true, true, b, false);
	ck(writes == 8, "and not again a second later");
	{
		mp4resume_seen fresh = { 0 };

		ck(!mp4resume_due(&fresh, true, true, "", 0, true), "no file: nothing, even when asked");
	}
#undef STEP
}

int main(void)
{
	row();
	session();
	printf("mp4resume: %d of %d checks pass\n", checks - fails, checks);
	return fails ? 1 : 0;
}

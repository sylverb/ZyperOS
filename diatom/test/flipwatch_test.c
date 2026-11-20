/* src/flipwatch.c: missed refreshes climb the clocks one step at a time,
 * a second apart at least, never in a game's first two seconds, and stop at
 * the CPU and the GPU held at their tops. */
#include <stdio.h>

#include "flipwatch.h"

static int checks, fails;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) { fails++; printf("  FAIL %s\n", what); }
}

#define S(x) ((uint64_t)((x) * 1000000.0))

int main(void)
{
	flipwatch w;

	/* a light system: two level steps, then the clocks held */
	flipwatch_begin(&w, 5, S(10));
	ck(flipwatch_turn(&w, 5, false, S(13)) == FLIPWATCH_NONE, "no new miss: nothing");
	ck(flipwatch_turn(&w, 6, false, S(11)) == FLIPWATCH_NONE, "a miss in the game's first two seconds: nothing");
	ck(flipwatch_turn(&w, 6, false, S(13)) == FLIPWATCH_NONE, "and it is not held against the game after");
	ck(flipwatch_turn(&w, 7, false, S(13.5)) == FLIPWATCH_LEVEL_UP, "a miss after them: the level up");
	ck(flipwatch_turn(&w, 8, false, S(14.0)) == FLIPWATCH_NONE, "another within the second: nothing more");
	ck(flipwatch_turn(&w, 9, false, S(14.6)) == FLIPWATCH_LEVEL_UP, "one after it: the next level");
	ck(flipwatch_turn(&w, 10, true, S(14.9)) == FLIPWATCH_NONE, "at full, within the second: nothing yet");
	ck(flipwatch_turn(&w, 11, true, S(16)) == FLIPWATCH_HOLD, "at full: the CPU and the GPU held at their tops");
	ck(flipwatch_turn(&w, 12, true, S(17.2)) == FLIPWATCH_NONE, "once");
	ck(flipwatch_turn(&w, 20, true, S(30)) == FLIPWATCH_NONE, "and nothing past that");

	/* a game at full power from the start goes straight to the hold */
	flipwatch_begin(&w, 0, 0);
	ck(flipwatch_turn(&w, 1, true, S(3)) == FLIPWATCH_HOLD, "full from the start: held at the first miss");

	/* a burst climbs once */
	flipwatch_begin(&w, 0, 0);
	ck(flipwatch_turn(&w, 4, false, S(3)) == FLIPWATCH_LEVEL_UP, "four misses at once: one step");
	ck(flipwatch_turn(&w, 4, false, S(9)) == FLIPWATCH_NONE, "and none while no more come");

	/* the count may wrap; any change is new misses */
	flipwatch_begin(&w, 0xffffffffu, 0);
	ck(flipwatch_turn(&w, 0, false, S(3)) == FLIPWATCH_LEVEL_UP, "a count that wrapped is a miss");

	if (fails) {
		printf("flipwatch: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("flipwatch: %d checks passed\n", checks);
	return 0;
}

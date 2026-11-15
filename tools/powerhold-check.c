/* SPDX-License-Identifier: GPL-3.0-only */
/* MENU > Power Button's rule (src/powerhold.h): a press turns the handheld off
 * in Press mode, a hold of POWERHOLD_MS in Hold mode, once per time down, and
 * how far a hold has got for the pill. */
#include <stdio.h>

#include "../src/osdbar.h"
#include "../src/powerhold.h"

static int checks, failures;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) {
		failures++;
		fprintf(stderr, "FAIL: %s\n", what);
	}
}

int main(void)
{
	powerhold p = { 0 };
	const unsigned t0 = 5000, step = POWERHOLD_MS / OSDBAR_POWER_STEPS;

	/* Press, the default */
	ck(!powerhold_mode(), "Press is the default");
	ck(powerhold_key(&p, true, t0), "Press: the press is the turn-off");
	ck(!powerhold_key(&p, true, t0 + 10), "Press: a second down while down is nothing");
	ck(!powerhold_due(&p, t0 + POWERHOLD_MS * 3), "Press: holding it on is no second turn-off");
	ck(powerhold_permille(&p, t0 + 100) == -1 && powerhold_wait_ms(&p, t0) == -1,
	   "Press: no hold shows, nothing to wake for");
	ck(!powerhold_key(&p, false, t0 + 200), "Press: the let-go is nothing");
	ck(powerhold_key(&p, true, t0 + 300), "Press: the next press is the turn-off again");
	powerhold_key(&p, false, t0 + 400);

	/* Hold */
	powerhold_set_mode(true);
	ck(!powerhold_key(&p, true, t0), "Hold: the press is not the turn-off");
	ck(powerhold_permille(&p, t0) == 0, "Hold: the hold starts empty");
	ck(powerhold_permille(&p, t0 + POWERHOLD_MS / 2) == 500, "Hold: halfway is 500");
	ck(!powerhold_due(&p, t0 + POWERHOLD_MS - 1), "Hold: a millisecond short is not due");
	ck(powerhold_wait_ms(&p, t0 + POWERHOLD_MS - 30) == 30, "Hold: the wait is to its end");
	ck(powerhold_next_ms(&p, t0, OSDBAR_POWER_STEPS) == (int)step,
	   "Hold: the next segment is a step away at the start");
	ck(powerhold_next_ms(&p, t0 + step + 7, OSDBAR_POWER_STEPS) == (int)step - 7,
	   "Hold: and the rest of a step after it");
	ck(powerhold_next_ms(&p, t0 + POWERHOLD_MS - 3, OSDBAR_POWER_STEPS) == 3,
	   "Hold: and never past its end");
	ck(powerhold_due(&p, t0 + POWERHOLD_MS), "Hold: due at POWERHOLD_MS");
	ck(!powerhold_due(&p, t0 + POWERHOLD_MS + 500), "Hold: and only once");
	ck(powerhold_permille(&p, t0 + POWERHOLD_MS + 1) == -1, "Hold: done, no hold shows");
	ck(!powerhold_key(&p, false, t0 + POWERHOLD_MS + 600), "Hold: the let-go is nothing");

	/* Let go early */
	ck(!powerhold_key(&p, true, t0 + 10000), "Hold: a new press");
	powerhold_key(&p, false, t0 + 10300);
	ck(!powerhold_due(&p, t0 + 10000 + POWERHOLD_MS * 2), "Hold: let go early, nothing is due");
	ck(powerhold_permille(&p, t0 + 10400) == -1, "Hold: and nothing shows");

	/* Taken for something else: the charging screen's hold to start */
	powerhold_key(&p, true, t0 + 20000);
	powerhold_consume(&p);
	ck(!powerhold_due(&p, t0 + 20000 + POWERHOLD_MS), "Hold: a hold taken is no turn-off");
	ck(powerhold_permille(&p, t0 + 20100) == -1, "Hold: and shows nothing");
	powerhold_key(&p, false, t0 + 23000);
	powerhold_key(&p, true, t0 + 24000);
	ck(powerhold_due(&p, t0 + 24000 + POWERHOLD_MS), "Hold: the next hold turns it off");
	powerhold_key(&p, false, t0 + 27000);
	powerhold_consume(&p);
	powerhold_key(&p, true, t0 + 28000);
	ck(powerhold_due(&p, t0 + 28000 + POWERHOLD_MS), "Hold: a consume with the key up takes nothing");

	/* Across the clock's wrap */
	powerhold_key(&p, false, 0);
	powerhold_key(&p, true, 0xFFFFFF00u);
	ck(!powerhold_due(&p, 0xFFFFFF00u + 100), "Hold: no early turn-off across the wrap");
	ck(powerhold_due(&p, 0xFFFFFF00u + POWERHOLD_MS), "Hold: due across the wrap");

	printf("powerhold: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}

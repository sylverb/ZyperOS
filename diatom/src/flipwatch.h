/* What the frame loop does when frames reach the panel late though they were
 * ready on time: a refresh missed (diatom_port_refreshes_missed).
 *
 * A system's power line caps its clocks (RUN power=light or medium), and
 * Diatom lifts the cap for the rest of a game at the first sign it is short:
 * a turn that starts late, or sound dropped. Neither happens when the cost is
 * the display's: a shader drawing each frame on the GPU, after the core has
 * finished it on time. On the GKD Pixel 2 with MENU > Shader at CRT, Sonic
 * the Hedgehog on the Master System (power light, the GPU at most 200 MHz)
 * showed 30 new frames a second while the core ran 59.9 (2026-11-16): every
 * flip took two refreshes. The GPU held at 520 MHz gave 58.2, and the shader
 * off 59.9. Under the same shader, a GBA, a DS and a Genesis game lost 1.5 to
 * 2 refreshes a second, and the SNES, with no shader, none; with the CPU and
 * the GPU both held at their tops, a GBA game lost none.
 *
 * So each missed refresh climbs one step, and the next only after a second
 * of calm, so a burst climbs once and only misses that go on climb further:
 *
 *   FLIPWATCH_LEVEL_UP  the power level one step up (light, medium, full)
 *   FLIPWATCH_HOLD      at full: the CPU's and the GPU's clocks held at
 *                       their tops, together, as the port holds them for a
 *                       core that draws with the GPU (diatom_port_fast_clock)
 *
 * and stays there for the rest of the game, as a lifted level does. Under
 * CRT on the Pixel 2 that took Goof Troop (SNES, power medium) from 58.1 new
 * frames a second to 60.0, and Metroid Fusion (GBA, its line made light)
 * from 58.7-59.0 to 59.6-59.7 (2026-11-16). The first seconds of a game are not
 * judged: its first flips set the display.
 *
 * Plain C, no port: test/flipwatch_test.c (check-flipwatch). */
#ifndef DIATOM_FLIPWATCH_H
#define DIATOM_FLIPWATCH_H

#include <stdbool.h>
#include <stdint.h>

/* How long after a step the next may come, and after the game's start the
 * first */
#define FLIPWATCH_CALM_US  1000000
#define FLIPWATCH_START_US 2000000

typedef enum {
	FLIPWATCH_NONE,
	FLIPWATCH_LEVEL_UP,
	FLIPWATCH_HOLD,
} flipwatch_step;

typedef struct {
	unsigned seen;          /* the port's count at the last look */
	bool     held;          /* the clocks held at their tops */
	uint64_t calm_until;    /* no step before this */
} flipwatch;

/* A game's watch, from `start_us` and the port's count then */
void flipwatch_begin(flipwatch *w, unsigned missed, uint64_t start_us);

/* After a turn: the port's count of missed refreshes now, whether the game
 * runs at full power, and the time. The step to take, or none. */
flipwatch_step flipwatch_turn(flipwatch *w, unsigned missed, bool full, uint64_t now_us);

#endif

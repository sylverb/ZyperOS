/* SPDX-License-Identifier: GPL-3.0-only */
/* Border, a display row (src/border.h): its kinds, the words kept, Off for a
 * word that is none, the database key, what the row offers and steps round,
 * a kept kind the row cannot show reading Off, the room rule at each mode
 * on the Pixel 2's and the Brick's surfaces, and NextUI's tags. The rows on
 * the menus are check-menus'; what Diatom draws, diatom's check-border and
 * check-border-e2e. */
#include <stdio.h>
#include <string.h>

#include "../src/border.h"

static int checks, fails;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) { fails++; printf("  FAIL %s\n", what); }
}

int main(void)
{
	border_kind list[BORDER_KINDS];
	char key[32];

	ck(!strcmp(border_id(BORDER_OFF), "off") && !strcmp(border_id(BORDER_BLUR), "blur") &&
	   !strcmp(border_id(BORDER_OWN), "own"), "each kind its word");
	ck(border_parse("blur") == BORDER_BLUR && border_parse("own") == BORDER_OWN && border_parse("off") == BORDER_OFF,
	   "and read back");
	ck(border_parse(NULL) == BORDER_OFF && border_parse("") == BORDER_OFF && border_parse("Blur") == BORDER_OFF,
	   "a word that is none is Off, black as before");
	border_key("GB", key, sizeof key);
	ck(!strcmp(key, "border.GB"), "the system's own key, by its tag");

	const border_offer all = { true, true, true, true }, blur_own = { true, false, false, true };
	const border_offer own_only = { false, false, false, true }, blur_only = { true, false, false, false };
	const border_offer none = { false, false, false, false }, cut_only = { true, true, false, true };

	ck(border_offered(all, list) == 5 && list[0] == BORDER_OFF && list[1] == BORDER_BLUR &&
	   list[2] == BORDER_HANDHELD && list[3] == BORDER_WHOLE && list[4] == BORDER_OWN,
	   "Off, Blur, Handheld, Handheld Whole, Your Own, in that order");
	ck(border_offered(blur_own, list) == 3 && list[2] == BORDER_OWN, "no Handheld frames where none is drawn");
	ck(border_offered(cut_only, list) == 4 && list[2] == BORDER_HANDHELD && list[3] == BORDER_OWN &&
	   border_shown(BORDER_WHOLE, cut_only) == BORDER_OFF, "Handheld without Whole where Whole has no placement");
	ck(border_offered(own_only, list) == 2 && list[1] == BORDER_OWN, "no Blur where the device has none");
	ck(border_offered(blur_only, list) == 2 && list[1] == BORDER_BLUR, "no Your Own without a picture");
	ck(border_offered(none, list) == 1, "Off alone");
	ck(border_shown(BORDER_OWN, blur_only) == BORDER_OFF && border_shown(BORDER_BLUR, own_only) == BORDER_OFF &&
	   border_shown(BORDER_WHOLE, blur_own) == BORDER_OFF && border_shown(BORDER_HANDHELD, all) == BORDER_HANDHELD,
	   "a kept kind the row cannot show reads Off");
	ck(border_step(BORDER_OFF, 1, blur_own) == BORDER_BLUR && border_step(BORDER_BLUR, 1, blur_own) == BORDER_OWN &&
	   border_step(BORDER_OWN, 1, blur_own) == BORDER_OFF, "right steps round the three");
	ck(border_step(BORDER_OFF, -1, blur_own) == BORDER_OWN, "and left");
	ck(border_step(BORDER_BLUR, 1, all) == BORDER_HANDHELD && border_step(BORDER_HANDHELD, 1, all) == BORDER_WHOLE &&
	   border_step(BORDER_WHOLE, 1, all) == BORDER_OWN, "and through the frames");
	ck(border_step(BORDER_OFF, 1, own_only) == BORDER_OWN && border_step(BORDER_OWN, 1, own_only) == BORDER_OFF,
	   "round what is offered only");
	ck(border_step(BORDER_OWN, 1, blur_only) == BORDER_BLUR, "a kind gone steps from Off");
	ck(!strcmp(border_id(BORDER_HANDHELD), "handheld") && !strcmp(border_id(BORDER_WHOLE), "whole") &&
	   border_parse("whole") == BORDER_WHOLE, "the frames' words");

	/* The room, on the Pixel 2's 640x480 and the Brick's 1024x768 */
	ck(border_room("integer", 160, 144, 0, 640, 480), "a Game Boy at Sharp Pixels on the Pixel 2 (3x)");
	ck(border_room("integer", 160, 144, 0, 1024, 768), "and on the Brick (4x)");
	ck(border_room("integer", 240, 160, 0, 640, 480), "a GBA at Sharp Pixels (2x)");
	ck(!border_room("integer", 320, 240, 0, 640, 480), "a 320x240 game at 2x fills the Pixel 2: no room");
	ck(!border_room("stretch", 160, 144, 0, 640, 480), "Full Screen covers it");
	ck(!border_room("fill", 160, 144, 0, 640, 480), "and Fill");
	ck(border_room("aspect", 160, 144, 160.0 / 144.0, 640, 480), "Original Shape leaves the sides");
	ck(!border_room("aspect", 320, 240, 4.0 / 3.0, 640, 480), "but not for a 4:3 game");
	ck(border_room("integer", 0, 0, 0, 640, 480) && !border_room("stretch", 0, 0, 0, 640, 480),
	   "a picture not known yet: room unless the mode covers whatever it is");
	ck(!border_room("overscale", 0, 0, 0, 640, 480) && !border_room("fill", 0, 0, 0, 1024, 768),
	   "Sharp Pixels, Zoomed and Fill cover whatever it is");
	ck(!border_room("overscale", 160, 144, 0, 640, 480) && !border_room("overscale", 240, 160, 0, 1024, 768),
	   "Sharp Pixels, Zoomed covers a known picture too");
	ck(!border_room(NULL, 160, 144, 0, 640, 480) && !border_room("integer", 160, 144, 0, 0, 0), "no mode, no surface");

	ck(!strcmp(border_nextui_tag("NES"), "FC") && !strcmp(border_nextui_tag("PS1"), "PS") &&
	   !border_nextui_tag("GB") && !border_nextui_tag("SFC") && !border_nextui_tag(NULL),
	   "NextUI's tags where they differ from ours");
	if (fails) {
		printf("border: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("border: %d checks passed\n", checks);
	return 0;
}

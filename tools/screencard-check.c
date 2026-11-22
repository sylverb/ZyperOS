/* SPDX-License-Identifier: GPL-3.0-only */
/* Recents' Last Screen card (src/screencard.h): the picture scaled up whole
 * by the nearest pixel toward SC_HEIGHT, and the cover small in its lower
 * left corner over its shadow, the rest of the picture untouched. */
#include <stdio.h>

#include "../src/screencard.h"

/* The generated card is the launcher's (src/ui.c); none here */
SDL_Surface *ui_card_paint(const ui_card_order *o, bool away) { (void)o; (void)away; return NULL; }
void ui_card_free(SDL_Surface *s) { SDL_FreeSurface(s); }

static int checks, failures;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) {
		failures++;
		fprintf(stderr, "FAIL: %s\n", what);
	}
}

static SDL_Surface *solid(int w, int h, Uint32 argb)
{
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);

	SDL_FillRect(s, NULL, argb);
	return s;
}

static Uint32 at(SDL_Surface *s, int x, int y)
{
	return ((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x];
}

int main(void)
{
	SDL_Surface *gba = solid(240, 160, 0xff2040a0u), *snes = solid(256, 224, 0xff2040a0u);
	SDL_Surface *ds = solid(256, 384, 0xff2040a0u), *box = solid(100, 140, 0xffe02020u);
	SDL_Surface *wide = solid(400, 100, 0xffe02020u), *c;
	int ch, cw, x0, y0;

	/* Whole times toward 448: a GBA's 160 three times, a SNES's 224 twice,
	 * a DS's 384 once */
	c = screen_card_from(gba, NULL);
	ck(c && c->w == 720 && c->h == 480, "a GBA's picture three times whole");
	ck(c && at(c, 0, 0) == 0xff2040a0u && at(c, 719, 479) == 0xff2040a0u, "every pixel the picture's, opaque");
	SDL_FreeSurface(c);
	c = screen_card_from(snes, NULL);
	ck(c && c->w == 512 && c->h == 448, "a SNES's twice");
	SDL_FreeSurface(c);
	c = screen_card_from(ds, NULL);
	ck(c && c->w == 256 && c->h == 384, "a DS's two screens once");
	SDL_FreeSurface(c);

	/* The cover: 30% of the card's height, inset 4% from the left and the
	 * foot, over a shadow down and right; the rest untouched */
	c = screen_card_from(gba, box);
	ch = 480 * SC_COVER_SHARE / 100;
	cw = 100 * ch / 140;
	x0 = 480 * SC_INSET_SHARE / 100;
	y0 = 480 - x0 - ch;
	ck(c && at(c, x0 + cw / 2, y0 + ch / 2) == 0xffe02020u, "the cover in the lower left corner");
	ck(c && at(c, x0 + 2, y0 - 2) == 0xff2040a0u, "the picture above it untouched");
	ck(c && at(c, 700, 20) == 0xff2040a0u, "and the rest of the picture");
	{
		Uint32 sh = c ? at(c, x0 + cw + 1, y0 + ch / 2) : 0;

		ck(c && sh != 0xff2040a0u && ((sh >> 16) & 0xff) < 0x20 && (sh & 0xff) < 0xa0,
		   "its shadow at its right, darker than the picture");
		ck(c && at(c, x0, y0) != 0xffe02020u, "its corner round");
	}
	SDL_FreeSurface(c);
	/* A wide box no wider than 45% of the card */
	c = screen_card_from(gba, wide);
	ck(c && at(c, x0 + 720 * 45 / 100 - 4, 480 - x0 - 10) == 0xffe02020u &&
	   at(c, x0 + 720 * 45 / 100 + 12, 480 - x0 - 10) != 0xffe02020u, "a wide box held to 45% of the card");
	SDL_FreeSurface(c);
	ck(screen_card_make("/nonexistent/x.auto.bmp", NULL, NULL, true) == NULL, "no picture: no card");

	SDL_FreeSurface(gba);
	SDL_FreeSurface(snes);
	SDL_FreeSurface(ds);
	SDL_FreeSurface(box);
	SDL_FreeSurface(wide);
	printf("screencard: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}

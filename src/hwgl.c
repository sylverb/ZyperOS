/* SPDX-License-Identifier: GPL-3.0-only */
/* The GL renderer and version for the hardware report (hwreport.h), apart
 * from hwreport.c so its check needs no SDL. */
#include "hwreport.h"

#include <SDL.h>
#include <stdio.h>

static char g_gl[160];

void hw_note_gl(void)
{
	typedef const unsigned char *(*get_string)(unsigned);
	get_string gs = (get_string)SDL_GL_GetProcAddress("glGetString");
	const unsigned char *r = gs ? gs(0x1F01) : NULL;   /* GL_RENDERER */
	const unsigned char *v = gs ? gs(0x1F02) : NULL;   /* GL_VERSION */

	snprintf(g_gl, sizeof g_gl, "%s%s%s", r ? (const char *)r : "", r && v ? ", " : "",
	         v ? (const char *)v : "");
}

const char *hw_gl(void)
{
	return g_gl;
}

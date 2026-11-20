/* The GL objects the presenting thread draws with, made on its context as
 * gl_start's last part: the plain and solid programs, and the frame's, the
 * overlay's, the level bar's and the FPS readout's textures, the BGRA ones
 * swizzled. Cut from the GKD Pixel 2's gl_start (port/pixel2.c), which
 * includes it in the function's body where it was cut, so its program is the
 * same bytes; the TrimUI Brick's gl_start includes it the same way. Needs a
 * bool function around it: it returns false when a program does not link. */
	g_prog_plain = link_program(PLAIN_SHADER);
	g_prog_solid = link_program(SOLID_SHADER);
	if (!g_prog_plain || !g_prog_solid) return false;

	glGenTextures(1, &g_frame_tex);
	glGenTextures(1, &g_overlay_tex);
	glGenTextures(1, &g_osdbar_tex);
	glGenTextures(1, &g_hud_tex);
	glGenTextures(1, &g_border_tex);
	/* The Border: BGRA swizzled back, opaque whatever its alpha, smooth
	 * both ways (the blur is 32x24 stretched to the panel) */
	glBindTexture(GL_TEXTURE_2D, g_border_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
	/* A Border set before this context was made (the Brick starts its GPU
	 * path late), uploaded at its first frame */
	g_bd_tex_w = g_bd_tex_h = 0;
	g_bd_dirty = g_bd != NULL;
	glBindTexture(GL_TEXTURE_2D, g_frame_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* The overlay is BGRA straight alpha, uploaded as bytes: swizzled back. */
	glBindTexture(GL_TEXTURE_2D, g_overlay_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* Linear going down (ov_scale, 0.625 here), as the launcher's own text
	 * is scaled to this panel; at 1, on whole pixels, it is the same */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	/* The level bar: the same, BGRA straight alpha (draw_level_bar). */
	glBindTexture(GL_TEXTURE_2D, g_osdbar_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	/* The FPS readout: the same, BGRA straight alpha (draw_hud) */
	glBindTexture(GL_TEXTURE_2D, g_hud_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	glViewport(0, 0, g_panel_w, g_panel_h);

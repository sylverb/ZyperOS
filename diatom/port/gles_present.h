/* The presenting thread and the frames' port interface Diatom's GPU ports
 * share: a capture of the front frame, the thread that draws each frame of
 * the mailbox (port/gles_state.h) with port/gles_draw.h and hands it to the
 * display, drain, and diatom_port_planes, _strength, _present, _overlay and
 * _present_stop, with the GPU frames' interface (port/gles_gpu.h) where the
 * Pixel 2 had it. Cut from the GKD Pixel 2's port (port/pixel2.c), which
 * includes it at the place it was cut from, so its program is the same
 * bytes; the TrimUI Brick's port includes it for its GPU path.
 *
 * What the including port defines before it, where the two devices differ:
 *   gl_start(), gl_stop()  the EGL display, context and surface, on the thread
 *   show_frame()           what was drawn onto the panel (a KMS flip, a pan)
 *   give_display()         the display handed back at a stop
 *   PRESENT_TARGET()       where the next frame is drawn: nothing on the Pixel
 *                          2 (its window surface), a page's framebuffer object
 *                          on the Brick
 *   PRESENT_SWAP()         the frame drawn: eglSwapBuffers on the Pixel 2, a
 *                          fence waited on the Brick
 *   PRESENT_TOP_DOWN       defined where the target keeps its first row in
 *                          memory at the top of the panel, the Brick's pages:
 *                          the drawing's y goes the other way
 *                          (port/gles_draw.h's to_clip), and so does a
 *                          capture's, which then reads its rows in order. Not
 *                          on the Pixel 2, whose window surface is GL's way up
 *   PORT_FN(n)             the interface's names: diatom_port_<n> on the Pixel
 *                          2; the Brick's own names, as it keeps its CPU path
 *                          beside this one
 *   osd_look_now(l)        the level bar's look now, under g_mx: the host's
 *                          (diatom_port_level_look) or the plain bar */

static bool capture_front(int front, const char *path)
{
	GLuint fbo, rb;
	uint8_t *rgba, *flipped;
	SDL_Surface *s, *rgb;
	size_t row = (size_t)g_surface_w * 4;
	bool ok = false;
	int y;

	if (front < 0) return false;
	rgba = malloc(row * (size_t)g_surface_h);
	flipped = malloc(row * (size_t)g_surface_h);
	if (!rgba || !flipped) { free(rgba); free(flipped); return false; }

	glGenFramebuffers(1, &fbo);
	glGenRenderbuffers(1, &rb);
	glBindRenderbuffer(GL_RENDERBUFFER, rb);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, g_surface_w, g_surface_h);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
	glViewport(0, 0, g_surface_w, g_surface_h);

	upload_frame(&g_frames[front]);
	draw_scene(&g_frames[front], false, false, false, NULL, 0, 0, 0, false,
	           false, false, NULL, NULL, NULL, false);
	glReadPixels(0, 0, g_surface_w, g_surface_h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, g_panel_w, g_panel_h);
	glDeleteRenderbuffers(1, &rb);
	glDeleteFramebuffers(1, &fbo);

#ifdef PRESENT_TOP_DOWN
	/* Drawn first row at y 0, as into the port's own target: GL reads the
	 * top row first */
	(void)y;
	memcpy(flipped, rgba, row * (size_t)g_surface_h);
#else
	/* GL reads bottom row first. */
	for (y = 0; y < g_surface_h; y++)
		memcpy(flipped + (size_t)y * row, rgba + (size_t)(g_surface_h - 1 - y) * row, row);
#endif

	s = SDL_CreateRGBSurfaceWithFormatFrom(flipped, g_surface_w, g_surface_h, 32,
	                                       (int)row, SDL_PIXELFORMAT_ABGR8888);
	if (s) {
		/* Plain 24-bit, as on the Brick: readers refuse 32-bit BMP headers. */
		rgb = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGB24, 0);
		if (rgb) { ok = SDL_SaveBMP(rgb, path) == 0; SDL_FreeSurface(rgb); }
		SDL_FreeSurface(s);
	}
	free(rgba);
	free(flipped);
	return ok;
}

static void *present_thread(void *arg)
{
	bool ok = gl_start();

	pthread_mutex_lock(&g_mx);
	g_thread_ready = ok ? 1 : -1;
	pthread_cond_broadcast(&g_idle);
	if (!ok) { pthread_mutex_unlock(&g_mx); gl_stop(); return arg; }

	while (!g_thread_stop) {
		int frame;
		bool ov_live, osd_live, lowbat, hud_live, hud_repaint, bd_live;
		int osd_theme;
		osdbar_look osd_look;
		float osd_alpha = 0, osd_rise = 0;
		uint64_t now, t0 = 0;
		EGLSyncKHR done;
		char hud_text_now[sizeof g_hud_text];
		ds_theme hud_colors;
		hud_style hud_st;

		if (g_pending < 0 && !g_want_blank && !g_capture_path) {
			pthread_cond_wait(&g_cv, &g_mx);
			continue;
		}

		if (g_capture_path) {
			const char *path = g_capture_path;
			int front = g_front;

			pthread_mutex_unlock(&g_mx);
			ok = capture_front(front, path);
			pthread_mutex_lock(&g_mx);
			g_capture_ok = ok;
			g_capture_path = NULL;
			pthread_cond_broadcast(&g_idle);
			continue;
		}

		if (g_want_blank) {
			g_want_blank = false;
			g_blanking = true;
			pthread_mutex_unlock(&g_mx);
			PRESENT_TARGET();
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			PRESENT_SWAP();
			show_frame();
			pthread_mutex_lock(&g_mx);
			g_blanking = false;
			pthread_cond_broadcast(&g_idle);
			continue;
		}

		frame = g_pending;
		g_pending  = -1;
		g_inflight = frame;

		/* The overlay is read here, under the lock the host's next
		 * diatom_port_overlay takes, so the borrowed pointer is never read
		 * after the host has moved on. */
		now = diatom_port_now_us();
		/* The Border, uploaded here when it moved, on this thread, whose
		 * context the texture is */
		bd_live = g_bd_on;
		if (bd_live && g_bd_dirty) {
			glBindTexture(GL_TEXTURE_2D, g_border_tex);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_bd_w, g_bd_h, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, g_bd);
			g_bd_tex_w = g_bd_w;
			g_bd_tex_h = g_bd_h;
			g_bd_dirty = false;
		}
		ov_live = g_ov && now < g_ov_until;
		if (ov_live && g_ov_dirty) {
			glBindTexture(GL_TEXTURE_2D, g_overlay_tex);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_ov_w, g_ov_h, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, g_ov);
			g_ov_dirty = false;
		}
		hold_tick(now);
		osd_live  = g_osd_on &&
		            osdbar_envelope((uint32_t)((now - g_osd_shown) / 1000),
		                            (uint32_t)((now - g_osd_changed) / 1000),
		                            &osd_alpha, &osd_rise);
		if (!osd_live) g_osd_on = false;
		/* A copy, so the host's next look can land while this one draws */
		osd_look_now(&osd_look);
		osd_theme = g_osd_theme;
		/* The look's colors for the bar, handed over once each time they
		 * move, on this thread, which is the one that paints the bar */
		if (g_osd_colors_gen != g_osd_colors_seen) {
			osdbar_colors(osd_theme, &g_osd_colors);
			g_osd_colors_seen = g_osd_colors_gen;
		}
		/* Blinking: the even seconds, the launcher's rhythm. */
		lowbat    = g_lowbat && (!g_lowbat_blink || (now / 1000000u) % 2 == 0);
		/* On-Screen Stats' line, style and colors, copied when they
		 * moved, for draw_hud to paint again outside the lock */
		hud_live = g_hud_on;
		hud_repaint = hud_live && g_hud_dirty;
		if (hud_repaint) {
			memcpy(hud_text_now, g_hud_text, sizeof hud_text_now);
			hud_colors = g_hud_colors;
			hud_st = g_hud_style;
			g_hud_dirty = false;
		}
		pthread_mutex_unlock(&g_mx);

		if (g_present_debug) t0 = diatom_port_now_us();
		gpu_frame_ready(&g_frames[frame]);
		if (g_present_debug && g_frames[frame].gpu) {
			static uint64_t waited, last;
			static int n;
			uint64_t t = diatom_port_now_us();

			waited += t - t0;
			if (++n >= 60) {
				port_logf(DIATOM_LOG_INFO, "present: 60 GPU frames in %llu ms, %llu ms of it "
				          "waiting for the core's drawing",
				          (unsigned long long)((t - last) / 1000),
				          (unsigned long long)(waited / 1000));
				n = 0;
				waited = 0;
				last = t;
			}
		}
		PRESENT_TARGET();
		upload_frame(&g_frames[frame]);
		draw_scene(&g_frames[frame], bd_live, ov_live, osd_live, &osd_look,
		           osd_theme, osd_alpha, osd_rise, lowbat,
		           hud_live, hud_repaint, hud_text_now, &hud_st, &hud_colors, g_turned);
		/* The core may draw into this texture again once the GPU has read
		 * it: a fence for diatom_port_gpu_target to wait on. */
		done = g_frames[frame].gpu
		     ? p_create_sync(g_egl_display, EGL_SYNC_FENCE_KHR, NULL) : EGL_NO_SYNC_KHR;
		if (g_frames[frame].gpu) {
			/* Drawn and on its way: the core may start its next frame */
			glFlush();
			pthread_mutex_lock(&g_mx);
			g_gpu_blitted = g_frames[frame].seq;
			pthread_cond_broadcast(&g_idle);
			pthread_mutex_unlock(&g_mx);
		}
		PRESENT_SWAP();
		if (g_present_debug) {
			static int n;
			uint64_t t1 = diatom_port_now_us();
			if (++n >= 60) {
				n = 0;
				port_logf(DIATOM_LOG_DEBUG, "present: upload+draw+swap %llu us",
				          (unsigned long long)(t1 - t0));
			}
		}
		show_frame();

		pthread_mutex_lock(&g_mx);
		g_front    = frame;
		g_inflight = -1;
		if (done != EGL_NO_SYNC_KHR) {
			if (g_gpu_done[frame] != EGL_NO_SYNC_KHR)
				p_destroy_sync(g_egl_display, g_gpu_done[frame]);
			g_gpu_done[frame] = done;
		}
		pthread_cond_broadcast(&g_idle);
	}
	pthread_mutex_unlock(&g_mx);
	gl_stop();
	return arg;
}

/* Until nothing is pending, nothing is being drawn and no request is open.
 * The thread keeps running: callers want quiescence, not teardown. */
static void drain(void)
{
	pthread_mutex_lock(&g_mx);
	while (g_thread_running &&
	       (g_pending >= 0 || g_inflight >= 0 || g_want_blank || g_blanking ||
	        g_capture_path))
		pthread_cond_wait(&g_idle, &g_mx);
	pthread_mutex_unlock(&g_mx);
}

/* ==================== the port interface ==================================== */

/* Set from the host's thread and read only there, by present, which copies
 * them into the frame it fills: the flip thread never sees these. */
static diatom_plane g_planes[DIATOM_PLANES_MAX];
static int          g_nplanes;

void PORT_FN(planes)(const diatom_plane *planes, int n)
{
	if (!planes || n < 0) n = 0;
	if (n > DIATOM_PLANES_MAX) n = DIATOM_PLANES_MAX;
	if (n) memcpy(g_planes, planes, sizeof *planes * (size_t)n);
	g_nplanes = n;
	port_logf(DIATOM_LOG_INFO, "present: %d plane(s)%s", n, n ? "" : ", the whole frame");
}

/* The old screens' strength, held like the planes and copied into each frame
 * present fills: host's thread only */
static int g_strength = 100;

void PORT_FN(strength)(int percent)
{
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;
	if (percent != g_strength)
		port_logf(DIATOM_LOG_INFO, "present: strength %d%%", percent);
	g_strength = percent;
}

void PORT_FN(present)(const void *src, int w, int h, size_t pitch,
                         diatom_pixfmt fmt, diatom_rect dst, diatom_filter filter)
{
	mailbox_frame *f;
	size_t need;
	int i;

	/* Dupe frame: the panel already shows it. */
	if (!src || w <= 0 || h <= 0 || !g_thread_running) return;

	pthread_mutex_lock(&g_mx);
	for (i = 0; i < MAILBOX_FRAMES; i++)
		if (i != g_front && i != g_inflight && i != g_pending && i != g_gpu_drawing) break;
	if (i == MAILBOX_FRAMES) {
		i = g_pending;
		g_pending = -1;
		drop_fence(i);
	}
	pthread_mutex_unlock(&g_mx);
	/* Every slot busy and none pending: the core holds one to draw into. A
	 * GPU core's session presents no pixels, so this is never its frame. */
	if (i < 0) return;

	f = &g_frames[i];
	f->gpu = false;
	need = pitch * (size_t)h;
	if (need > f->cap) {
		/* Grows only, and only when a core's geometry outgrows every frame so
		 * far: a few times per game at most, never per frame. */
		uint8_t *p = realloc(f->pixels, need);
		if (!p) {
			diatom_port_log(DIATOM_LOG_WARN, "present: no memory for the frame; dropped");
			return;
		}
		f->pixels = p;
		f->cap = need;
	}
	/* What the port was handed, once per change, as brick.c logs it. Against
	 * the last frame logged, not the slot's old contents: the slots rotate. */
	{
		static int lw = -1, lh = -1;
		static size_t lpitch;
		static diatom_rect ldst;
		static diatom_filter lfilter;

		if (w != lw || h != lh || pitch != lpitch || filter != lfilter ||
		    memcmp(&dst, &ldst, sizeof dst) != 0) {
			lw = w; lh = h; lpitch = pitch; ldst = dst; lfilter = filter;
			port_logf(DIATOM_LOG_INFO,
			          "present: src %dx%d pitch %zu fmt %d -> dst %dx%d at %d,%d "
			          "(surface %dx%d) %s",
			          w, h, pitch, (int)fmt, dst.w, dst.h, dst.x, dst.y,
			          g_surface_w, g_surface_h,
			          filter == DIATOM_FILTER_SHARP ? "sharp" : "nearest");
		}
	}
	memcpy(f->pixels, src, need);
	f->w = w;
	f->h = h;
	f->pitch = pitch;
	f->fmt = fmt;
	f->dst = dst;
	f->filter = filter;
	f->strength = g_strength;
	f->nplanes = g_nplanes;
	if (g_nplanes) memcpy(f->planes, g_planes, sizeof g_planes[0] * (size_t)g_nplanes);

	pthread_mutex_lock(&g_mx);
	g_presented = true;
	g_pending = i;
	pthread_cond_signal(&g_cv);
	pthread_mutex_unlock(&g_mx);
}

#include "gles_gpu.h"

static float ov_scale(void)
{
	return g_surface_w > 0 && g_surface_w < 1024 ? (float)g_surface_w / 1024.0f : 1.0f;
}

void PORT_FN(border)(const uint8_t *bgra, int w, int h)
{
	uint8_t *copy = NULL;

	/* Copied outside the lock: the present thread waits on it */
	if (bgra && w > 0 && h > 0) {
		copy = malloc((size_t)w * (size_t)h * 4);
		if (copy) memcpy(copy, bgra, (size_t)w * (size_t)h * 4);
	}
	pthread_mutex_lock(&g_mx);
	if (copy && g_bd && g_bd_w == w && g_bd_h == h) {
		/* The same size again (the blur, ten times a second): into the
		 * buffer that is there, and the copy let go */
		memcpy(g_bd, copy, (size_t)w * (size_t)h * 4);
	} else {
		free(g_bd);
		g_bd = copy;
		copy = NULL;
		g_bd_w = g_bd ? w : 0;
		g_bd_h = g_bd ? h : 0;
	}
	g_bd_on = g_bd != NULL;
	g_bd_dirty = g_bd_on;
	pthread_mutex_unlock(&g_mx);
	free(copy);
}

void PORT_FN(overlay)(const uint8_t *bgra, int w, int h, unsigned ms)
{
	pthread_mutex_lock(&g_mx);
	if (!bgra || ms == 0 || w <= 0 || h <= 0) {
		g_ov = NULL;
		g_ov_until = 0;
	} else if (roundf(w * ov_scale()) > g_surface_w || roundf(h * ov_scale()) > g_surface_h) {
		/* Refused, not clipped: see diatom_port.h. */
		diatom_port_log(DIATOM_LOG_WARN, "overlay larger than the surface; ignored");
	} else {
		g_ov = bgra;
		g_ov_w = w;
		g_ov_h = h;
		g_ov_until = diatom_port_now_us() + (uint64_t)ms * 1000ull;
		g_ov_dirty = true;
	}
	pthread_mutex_unlock(&g_mx);
}

/* The pixels of the frames that are not on glass: copies the flip thread has
 * already uploaded, each as large as the largest frame the game drew (two of
 * them are 2.5 MB after a 640x480 XRGB8888 game), which nothing reads until a
 * present fills them again and grows them back. Let go when a game stops
 * presenting rather than kept through the launcher's whole stay. The front
 * one stays: a capture of what is on glass uploads it again. */
static void release_frames(void)
{
	int i;

	pthread_mutex_lock(&g_mx);
	for (i = 0; i < MAILBOX_FRAMES; i++)
		if (i != g_front && i != g_pending && i != g_inflight && i != g_gpu_drawing) {
			free(g_frames[i].pixels);
			g_frames[i].pixels = NULL;
			g_frames[i].cap = 0;
		}
	pthread_mutex_unlock(&g_mx);
}

void PORT_FN(present_stop)(diatom_park park)
{
	bool was_presenting;

	if (!g_thread_running) return;
	drain();

	pthread_mutex_lock(&g_mx);
	was_presenting = g_presented;
	g_presented = false;
	pthread_mutex_unlock(&g_mx);

	/* Nothing presented since the last stop: the launcher has the display,
	 * and parking would take it (the Brick learned this the hard way, see
	 * brick.c). */
	if (!was_presenting) return;

	if (park == DIATOM_PARK_BLANK) {
		pthread_mutex_lock(&g_mx);
		g_want_blank = true;
		pthread_cond_signal(&g_cv);
		pthread_mutex_unlock(&g_mx);
		drain();
	}
	/* KEEP leaves the last frame on glass, and the buffer behind it stays
	 * allocated: removing a framebuffer that is on glass blanks the panel.
	 * The launcher's first flip replaces it. */
	release_frames();
	give_display();
}

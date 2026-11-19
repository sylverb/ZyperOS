/* The GPU frames' port interface Diatom's GPU ports share
 * (diatom_port_gpu_begin, _end, _target, _proc, _present_gpu, _gpu_read): a
 * core that draws with the GPU, on a context of its own that shares the
 * presenting thread's textures, into the mailbox's slots, kept apart by
 * fences (port/gles_state.h). Cut from the GKD Pixel 2's port
 * (port/pixel2.c), which includes it at the place it was cut from, so its
 * program is the same bytes; the TrimUI Brick's port includes it for its
 * GPU path. Its log lines start with PORT_NAME. Definitions, for the one
 * file that includes it, which has the mailbox and its thread. */

/* ---------- GPU frames: the port interface ------------------------------------
 * See the GPU frames section near the top for the slots and the fences. */

bool diatom_port_gpu_begin(const diatom_gpu_spec *spec)
{
	EGLint attribs[] = { EGL_CONTEXT_MAJOR_VERSION, spec->major >= 3 ? 3 : 2, EGL_NONE };
	GLint max_tex = 0;
	int i;

	if (g_gpu_ctx != EGL_NO_CONTEXT) diatom_port_gpu_end();
	if (!g_thread_running || g_egl_context == EGL_NO_CONTEXT || !sync_procs()) {
		diatom_port_log(DIATOM_LOG_ERROR, PORT_NAME ": no GPU for the core (no display or no fences)");
		return false;
	}
	/* Sharing the thread's context: its textures are the core's targets */
	g_gpu_ctx = eglCreateContext(g_egl_display, g_egl_config, g_egl_context, attribs);
	if (g_gpu_ctx == EGL_NO_CONTEXT && attribs[1] == 2) {
		attribs[1] = 3;                 /* OpenGL ES 3 runs an ES 2 core as well */
		g_gpu_ctx = eglCreateContext(g_egl_display, g_egl_config, g_egl_context, attribs);
	}
	if (g_gpu_ctx == EGL_NO_CONTEXT ||
	    !eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, g_gpu_ctx)) {
		port_logf(DIATOM_LOG_ERROR, PORT_NAME ": no OpenGL ES %d context for the core (0x%x)",
		          attribs[1], eglGetError());
		if (g_gpu_ctx != EGL_NO_CONTEXT) eglDestroyContext(g_egl_display, g_gpu_ctx);
		g_gpu_ctx = EGL_NO_CONTEXT;
		return false;
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
	g_gpu_top_down = spec->top_down;
	g_gpu_w = spec->max_w > 0 ? spec->max_w : g_surface_w;
	g_gpu_h = spec->max_h > 0 ? spec->max_h : g_surface_h;
	if (max_tex > 0 && g_gpu_w > max_tex) g_gpu_w = max_tex;
	if (max_tex > 0 && g_gpu_h > max_tex) g_gpu_h = max_tex;

	/* One depth and stencil buffer for every slot: the core draws one frame
	 * at a time, and nothing of a frame's depth outlives it. */
	g_gpu_depth = 0;
	if (spec->depth || spec->stencil) {
		glGenRenderbuffers(1, &g_gpu_depth);
		glBindRenderbuffer(GL_RENDERBUFFER, g_gpu_depth);
		glRenderbufferStorage(GL_RENDERBUFFER,
		                      spec->stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16,
		                      g_gpu_w, g_gpu_h);
	}
	glGenTextures(MAILBOX_FRAMES, g_gpu_tex);
	for (i = 0; i < MAILBOX_FRAMES; i++) {
		glBindTexture(GL_TEXTURE_2D, g_gpu_tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_gpu_w, g_gpu_h, 0,
		             GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	}
	glGenFramebuffers(1, &g_gpu_read_fb);
	glGenFramebuffers(1, &g_gpu_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, g_gpu_fb);
	/* Depth and stencil apart, which OpenGL ES 2 takes as well as 3 */
	if (g_gpu_depth)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		                          GL_RENDERBUFFER, g_gpu_depth);
	if (g_gpu_depth && spec->stencil)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
		                          GL_RENDERBUFFER, g_gpu_depth);
	/* Each slot's texture through it once: cleared, and checked complete */
	for (i = 0; i < MAILBOX_FRAMES; i++) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		                       g_gpu_tex[i], 0);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			diatom_port_log(DIATOM_LOG_ERROR, PORT_NAME ": the core's framebuffer is incomplete");
			diatom_port_gpu_end();
			return false;
		}
	}
	g_gpu_attached = MAILBOX_FRAMES - 1;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFinish();

	pthread_mutex_lock(&g_mx);
	g_gpu_drawing = -1;
	g_gpu_seq = g_gpu_blitted = 0;
	pthread_mutex_unlock(&g_mx);
	g_gpu_last = -1;
	port_logf(DIATOM_LOG_INFO, PORT_NAME ": GPU frames on OpenGL ES %d, up to %dx%d%s%s",
	          attribs[1], g_gpu_w, g_gpu_h, spec->depth ? ", depth" : "",
	          spec->stencil ? ", stencil" : "");
	return true;
}

void diatom_port_gpu_end(void)
{
	int i;

	if (g_gpu_ctx == EGL_NO_CONTEXT) return;
	/* Nothing of the core's may still be on its way to the panel, and the
	 * front frame, once its texture is gone, may never be drawn again. */
	drain();
	pthread_mutex_lock(&g_mx);
	for (i = 0; i < MAILBOX_FRAMES; i++) {
		drop_fence(i);
		if (g_gpu_done[i] != EGL_NO_SYNC_KHR) {
			p_destroy_sync(g_egl_display, g_gpu_done[i]);
			g_gpu_done[i] = EGL_NO_SYNC_KHR;
		}
	}
	if (g_front >= 0 && g_frames[g_front].gpu) g_front = -1;
	for (i = 0; i < MAILBOX_FRAMES; i++) g_frames[i].gpu = false;
	g_gpu_drawing = -1;
	pthread_mutex_unlock(&g_mx);
	g_gpu_last = -1;
	if (g_gpu_throttle != EGL_NO_SYNC_KHR) {
		p_destroy_sync(g_egl_display, g_gpu_throttle);
		g_gpu_throttle = EGL_NO_SYNC_KHR;
	}

	eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, g_gpu_ctx);
	glDeleteFramebuffers(1, &g_gpu_fb);
	glDeleteFramebuffers(1, &g_gpu_read_fb);
	glDeleteTextures(MAILBOX_FRAMES, g_gpu_tex);
	if (g_gpu_depth) glDeleteRenderbuffers(1, &g_gpu_depth);
	memset(g_gpu_tex, 0, sizeof g_gpu_tex);
	g_gpu_fb = g_gpu_read_fb = g_gpu_depth = 0;
	g_gpu_attached = -1;
	eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(g_egl_display, g_gpu_ctx);
	g_gpu_ctx = EGL_NO_CONTEXT;
}

uintptr_t diatom_port_gpu_target(void)
{
	EGLSyncKHR done = EGL_NO_SYNC_KHR;
	int i;

	if (g_gpu_ctx == EGL_NO_CONTEXT) return 0;
	pthread_mutex_lock(&g_mx);
	if (g_gpu_drawing < 0) {
		/* Turn-taking: the next frame starts once the thread has drawn the
		 * last one to the panel. The GPU runs one job after another, so a
		 * frame begun earlier went ahead of the thread's small one, and the
		 * panel waited behind the core: Crazy Taxi 2's 27 frames a second
		 * reached it as 11 (2026-11-04). Bounded, so a thread that stopped
		 * drawing cannot hold the core for good. */
		struct timespec until;

		clock_gettime(CLOCK_MONOTONIC, &until);   /* g_idle's clock, set at init */
		until.tv_nsec += 200 * 1000000L;
		if (until.tv_nsec >= 1000000000L) { until.tv_sec++; until.tv_nsec -= 1000000000L; }
		while (g_thread_running && g_gpu_blitted != g_gpu_seq)
			if (pthread_cond_timedwait(&g_idle, &g_mx, &until) == ETIMEDOUT) break;

		for (i = 0; i < MAILBOX_FRAMES; i++)
			if (i != g_front && i != g_inflight && i != g_pending) break;
		if (i == MAILBOX_FRAMES) {      /* all three taken: the newest wins */
			i = g_pending;
			g_pending = -1;
			drop_fence(i);
		}
		g_gpu_drawing = i;
		done = g_gpu_done[i];
		g_gpu_done[i] = EGL_NO_SYNC_KHR;
	}
	i = g_gpu_drawing;
	pthread_mutex_unlock(&g_mx);
	wait_sync(done);
	if (i != g_gpu_attached) {
		/* The core's own binding put back: its state cache expects it */
		GLint prev = 0;

		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
		glBindFramebuffer(GL_FRAMEBUFFER, g_gpu_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		                       g_gpu_tex[i], 0);
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
		g_gpu_attached = i;
	}
	return g_gpu_fb;
}

void *diatom_port_gpu_proc(const char *name)
{
	return (void *)eglGetProcAddress(name);
}

void diatom_port_present_gpu(int w, int h, diatom_rect dst, diatom_filter filter)
{
	mailbox_frame *f;
	EGLSyncKHR fence, throttle;
	int i;

	if (g_gpu_ctx == EGL_NO_CONTEXT || w <= 0 || h <= 0 || !g_thread_running) return;
	pthread_mutex_lock(&g_mx);
	i = g_gpu_drawing;
	pthread_mutex_unlock(&g_mx);
	if (i < 0) return;              /* nothing drawn since the last one */
	if (w > g_gpu_w) w = g_gpu_w;
	if (h > g_gpu_h) h = g_gpu_h;

	/* Into the core's command stream after its drawing, and flushed: the
	 * thread's wait flushes only its own context. */
	fence = p_create_sync(g_egl_display, EGL_SYNC_FENCE_KHR, NULL);
	throttle = p_create_sync(g_egl_display, EGL_SYNC_FENCE_KHR, NULL);
	glFlush();
	/* And no further ahead of the GPU than one frame: the previous frame
	 * must be done before the core starts the next. Nothing else holds it
	 * back - RetroArch's swap does that there - and a core that queued
	 * frames faster than the GPU drew them had every frame drawn and only
	 * the newest shown, behind all the rest: Crazy Taxi 2 queued about 37 a
	 * second and the panel showed 6 (2026-11-04). */
	if (g_present_debug) {
		static uint64_t waited, last;
		static int n;
		uint64_t t0 = diatom_port_now_us(), t1;

		wait_sync(g_gpu_throttle);
		t1 = diatom_port_now_us();
		waited += t1 - t0;
		if (++n >= 60) {
			port_logf(DIATOM_LOG_INFO, "present: the core drew 60 frames in %llu ms, "
			          "%llu ms of it waiting for the GPU",
			          (unsigned long long)((t1 - last) / 1000),
			          (unsigned long long)(waited / 1000));
			n = 0;
			waited = 0;
			last = t1;
		}
	} else {
		wait_sync(g_gpu_throttle);
	}
	g_gpu_throttle = throttle;

	{
		static int lw = -1, lh = -1;

		if (w != lw || h != lh) {
			lw = w; lh = h;
			port_logf(DIATOM_LOG_INFO,
			          "present: GPU frame %dx%d of %dx%d -> dst %dx%d at %d,%d %s",
			          w, h, g_gpu_w, g_gpu_h, dst.w, dst.h, dst.x, dst.y,
			          filter == DIATOM_FILTER_SHARP ? "sharp" : "nearest");
		}
	}
	f = &g_frames[i];
	f->gpu = true;
	f->top_down = g_gpu_top_down;
	f->gpu_tex = g_gpu_tex[i];
	f->tex_w = g_gpu_w;
	f->tex_h = g_gpu_h;
	f->w = w;
	f->h = h;
	f->fmt = DIATOM_PIX_XRGB8888;
	f->dst = dst;
	f->filter = filter;
	f->strength = g_strength;
	f->fence = fence;
	f->seq = g_gpu_seq + 1;         /* g_gpu_seq is this thread's alone to change */
	f->nplanes = g_nplanes;
	if (g_nplanes) memcpy(f->planes, g_planes, sizeof g_planes[0] * (size_t)g_nplanes);
	g_gpu_last = i;

	pthread_mutex_lock(&g_mx);
	if (g_pending >= 0 && g_pending != i) drop_fence(g_pending);
	g_presented = true;
	g_pending = i;
	g_gpu_drawing = -1;
	g_gpu_seq++;
	pthread_cond_signal(&g_cv);
	pthread_mutex_unlock(&g_mx);
}

bool diatom_port_gpu_read(void *xrgb, int *w, int *h, size_t cap)
{
	const mailbox_frame *f;
	GLint prev_fb = 0, prev_align = 4;
	uint8_t *rgba;
	uint32_t *out = xrgb;
	int x, y, fw, fh;

	if (g_gpu_ctx == EGL_NO_CONTEXT || g_gpu_last < 0 || !xrgb) return false;
	f = &g_frames[g_gpu_last];
	fw = f->w;
	fh = f->h;
	if ((size_t)fw * (size_t)fh * 4 > cap) return false;
	rgba = malloc((size_t)fw * (size_t)fh * 4);
	if (!rgba) return false;

	/* The core's own bindings are put back: its state cache (glsm) does not
	 * expect them to move between frames. */
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fb);
	glGetIntegerv(GL_PACK_ALIGNMENT, &prev_align);
	glBindFramebuffer(GL_FRAMEBUFFER, g_gpu_read_fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                       g_gpu_tex[g_gpu_last], 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glPixelStorei(GL_PACK_ALIGNMENT, prev_align);
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fb);

	/* Bottom row first in GL (unless the core drew top down), top row first
	 * out; RGBA bytes to 0x00RRGGBB */
	for (y = 0; y < fh; y++) {
		int row = g_gpu_top_down ? y : fh - 1 - y;
		const uint8_t *s = rgba + (size_t)row * (size_t)fw * 4;
		uint32_t *d = out + (size_t)y * (size_t)fw;

		for (x = 0; x < fw; x++, s += 4)
			d[x] = (uint32_t)s[0] << 16 | (uint32_t)s[1] << 8 | s[2];
	}
	free(rgba);
	*w = fw;
	*h = fh;
	return true;
}

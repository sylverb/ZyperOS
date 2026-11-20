/* The GL state Diatom's GPU ports share: the EGL display, context and
 * config, the panel's and the surface's sizes, the mailbox of frames the
 * presenting thread draws (pixel frames and a GPU core's), the GL objects it
 * draws with, and the GPU frames' context, textures and fences with the KHR
 * sync calls. Cut from the GKD Pixel 2's port (port/pixel2.c), which includes
 * it at the place it was cut from, so its program is the same bytes; the
 * TrimUI Brick's port includes it for its GPU path. Definitions, static, for
 * the one file that includes it, as port/gles_draw.h. */
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLContext g_egl_context = EGL_NO_CONTEXT;
static EGLSurface g_egl_surface = EGL_NO_SURFACE;
static EGLConfig  g_egl_config;

/* The panel in its own terms, and the surface as the host sees it. On this
 * device the panel is 480x640 and the surface 640x480; g_turned says the two
 * differ. */
static int  g_panel_w, g_panel_h;
static int  g_surface_w, g_surface_h;
static bool g_turned;

/* Frames handed over by present(). Three, with the Brick's roles: front is
 * the last one drawn (kept for a capture), inflight is being drawn now,
 * pending is published and waiting. present() writes into whichever holds no
 * role and steals pending when all three are taken. Latest wins. */
#define MAILBOX_FRAMES 3
typedef struct {
	uint8_t      *pixels;
	size_t        cap;
	int           w, h;
	size_t        pitch;
	diatom_pixfmt fmt;
	diatom_rect   dst;
	diatom_filter filter;
	int           strength;     /* diatom_port_strength's when presented, percent */
	/* The planes in force when this frame was presented, so a layout change
	 * and the frame it was made for reach the panel together. */
	diatom_plane  planes[DIATOM_PLANES_MAX];
	int           nplanes;
	/* A GPU frame (present_gpu) is no pixels at all: the slot's texture,
	 * which the core drew into on its own context, tex_w x tex_h with the
	 * frame at its bottom-left, and the fence that says the drawing is done.
	 * The thread waits on the fence before it samples. */
	bool          gpu;
	bool          top_down;     /* a GPU frame drawn first row at the top */
	GLuint        gpu_tex;
	int           tex_w, tex_h;
	EGLSyncKHR    fence;
	unsigned      seq;          /* present_gpu's count, for the turn-taking */
} mailbox_frame;

static mailbox_frame    g_frames[MAILBOX_FRAMES];
static pthread_t        g_thread;
static pthread_mutex_t  g_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cv = PTHREAD_COND_INITIALIZER;     /* work for the thread */
static pthread_cond_t   g_idle = PTHREAD_COND_INITIALIZER;   /* the thread went idle */
static int              g_front = -1, g_inflight = -1, g_pending = -1;
static bool             g_thread_running, g_thread_stop;
static bool             g_want_blank;       /* present_stop(BLANK) asked for black */
static bool             g_blanking;
static const char      *g_capture_path;     /* a capture asked for, and its answer */
static bool             g_capture_ok;
static bool             g_presented;        /* anything presented since the last stop */

/* Startup handshake: the EGL context belongs to the thread, so the thread
 * builds it and init waits to hear whether that worked. */
static int              g_thread_ready;     /* 0 waiting, 1 ok, -1 failed */

static bool g_present_debug;

/* GL objects, all owned by the thread. */
static GLuint g_prog_plain, g_prog_solid;
/* The final pass's program for each filter but nearest (g_prog_plain),
 * linked the first time a frame asks for it (filter_prog); 0 until then, and
 * after a link that failed, which g_prog_failed keeps from being tried every
 * frame. */
static GLuint g_prog_filter[DIATOM_FILTER_COUNT];
static bool   g_prog_failed[DIATOM_FILTER_COUNT];
/* The same for an old screen drawn under 100% strength (diatom_port_strength):
 * its program with a u_strength, linked the first time a frame asks for it,
 * so a frame at 100% draws with the program above and nothing more. */
static GLuint g_prog_soft[DIATOM_FILTER_COUNT];
static bool   g_soft_failed[DIATOM_FILTER_COUNT];
static GLuint g_frame_tex, g_overlay_tex, g_osdbar_tex, g_hud_tex;
/* The Border (diatom_port_border): the host's picture, copied, uploaded on
 * the presenting thread when it moved, drawn under the game while on. Under
 * g_mx, as the overlay is. */
static GLuint   g_border_tex;
static uint8_t *g_bd;
static int      g_bd_w, g_bd_h;
static bool     g_bd_on, g_bd_dirty;
/* The size the texture holds, the presenting thread's own (set as it uploads) */
static int      g_bd_tex_w, g_bd_tex_h;
static int    g_tex_w = -1, g_tex_h = -1;
static diatom_pixfmt g_tex_fmt;

/* ---------- GPU frames ------------------------------------------------------- */

/* A core that draws with the GPU (diatom_port_gpu_begin) gets a context of its
 * own on the host's thread, sharing textures with the thread's, and one
 * texture per mailbox slot: slot i draws into texture g_gpu_tex[i], so a GPU
 * frame takes the same roles a pixel frame does (front, inflight, pending),
 * and the texture of a slot holding a role is never drawn into.
 *
 * ONE framebuffer for all of them, its texture swapped under it as the slots
 * turn. A core may read the framebuffer's name once and keep it: libretro's
 * GL state manager (glsm, which flycast2021 uses) takes it at context_reset
 * and binds that name every frame after. With a framebuffer per slot that
 * core drew every frame into slot 0's and the panel showed the others, black
 * (2026-11-04). Framebuffers are the core context's own; only the textures
 * are shared.
 *
 * Two fences keep the two contexts apart. present_gpu leaves one after the
 * core's drawing, which the thread waits on before sampling (the frame's
 * `fence`); the thread leaves one after sampling (g_gpu_done), which
 * gpu_target waits on before the core draws into that texture again. By then
 * it has nearly always passed: the slot was free, so the thread is two frames
 * on. */
static EGLContext g_gpu_ctx = EGL_NO_CONTEXT;
static GLuint     g_gpu_tex[MAILBOX_FRAMES], g_gpu_depth;
static GLuint     g_gpu_fb;                      /* the core draws into this one */
static GLuint     g_gpu_read_fb;                 /* gpu_read's, for any slot */
static int        g_gpu_attached = -1;           /* the slot whose texture g_gpu_fb holds */
static EGLSyncKHR g_gpu_done[MAILBOX_FRAMES];    /* under g_mx */
static int        g_gpu_w, g_gpu_h;
static bool       g_gpu_top_down;                /* the core draws its first row at the top */
static int        g_gpu_drawing = -1;            /* the slot the core draws into; under g_mx */
static int        g_gpu_last = -1;               /* the slot of the last GPU frame presented */
/* The end of the core's last frame on the GPU, for present_gpu's throttle */
static EGLSyncKHR g_gpu_throttle = EGL_NO_SYNC_KHR;
/* Turn-taking on the GPU (gpu_target): the last GPU frame presented, and the
 * last one the thread has drawn to the panel, by present_gpu's count. Under
 * g_mx. */
static unsigned   g_gpu_seq, g_gpu_blitted;

static PFNEGLCREATESYNCKHRPROC     p_create_sync;
static PFNEGLDESTROYSYNCKHRPROC    p_destroy_sync;
static PFNEGLCLIENTWAITSYNCKHRPROC p_client_wait;
static PFNEGLWAITSYNCKHRPROC       p_server_wait;   /* NULL without EGL_KHR_wait_sync */

static bool sync_procs(void)
{
	if (!p_create_sync) {
		const char *ext = eglQueryString(g_egl_display, EGL_EXTENSIONS);

		p_create_sync  = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
		p_destroy_sync = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
		p_client_wait  = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
		if (ext && strstr(ext, "EGL_KHR_wait_sync"))
			p_server_wait = (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
	}
	return p_create_sync && p_destroy_sync && p_client_wait;
}

static void wait_sync(EGLSyncKHR s)
{
	if (s == EGL_NO_SYNC_KHR) return;
	p_client_wait(g_egl_display, s, 0, EGL_FOREVER_KHR);
	p_destroy_sync(g_egl_display, s);
}

/* A frame dropped unseen (stolen from pending) leaves its fence unwaited. */
static void drop_fence(int slot)
{
	mailbox_frame *f;

	if (slot < 0) return;
	f = &g_frames[slot];
	if (f->gpu && f->fence != EGL_NO_SYNC_KHR) {
		p_destroy_sync(g_egl_display, f->fence);
		f->fence = EGL_NO_SYNC_KHR;
	}
}

/* The thread, before it samples a GPU frame: the core's drawing is done.
 *
 * Done on the GPU, which is where it has to be: the thread's commands wait
 * for the fence there (eglWaitSyncKHR), and the thread goes on to queue its
 * drawing at once. Waiting on the CPU instead held the thread, and through
 * the turn-taking in diatom_port_gpu_target the core with it, until the GPU
 * had finished the frame: the CPU and the GPU took turns, and a frame cost
 * the two together. Dr. Mario 64 drew one in 30 ms, 15 of them the thread
 * waiting here (2026-11-04). The order on the GPU is unchanged - the core's
 * frame, this thread's blit of it, the core's next frame - as the core starts
 * its next only once this thread has flushed. A client wait where the
 * extension is missing. */
static void gpu_frame_ready(mailbox_frame *f)
{
	if (!f->gpu || f->fence == EGL_NO_SYNC_KHR) return;
	if (p_server_wait) {
		p_server_wait(g_egl_display, f->fence, 0);
		p_destroy_sync(g_egl_display, f->fence);
	} else {
		wait_sync(f->fence);
	}
	f->fence = EGL_NO_SYNC_KHR;
}

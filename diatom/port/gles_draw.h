/* The GL drawing Diatom's GPU ports share: the shaders (the old screens and
 * their strength), compiling and linking them, the quads, a frame's upload,
 * the level bar, the FPS readout, the low-battery dot, the filter's program
 * and the scene. Written for the GKD Pixel 2's port (port/pixel2.c), which
 * includes it at the place it was cut from, so its program is the same
 * bytes as before; the TrimUI Brick's port includes it for its own GPU path.
 *
 * Not a header of declarations: definitions, static, for the one file that
 * includes it. Its log lines start with PORT_NAME, the including port's
 * name as a string ("pixel2"), so the Pixel 2's are the same strings. That file has, before the include, the state they draw from:
 * the GL objects (g_prog_*, g_*_tex), the panel and surface sizes, the
 * overlay, the level bar's and the HUD's state, the mailbox's frame type. */
static const char *VERTEX_SHADER =
	"#version 300 es\n"
	"in vec2 a_pos;\n"
	"in vec2 a_src;\n"
	"out vec2 v_src;\n"
	"void main() { v_src = a_src; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

/* Every program draws the frame's colors as the core made them: no color
 * matrix, no curve. Up to 1.0.4 they went through the panel's color matrix
 * from plastron's /etc/panel-color (blue gained a tenth of green, in linear
 * light), set by eye against a TrimUI Brick, which with the splash's gamma
 * table made games darker and cooler than the stock firmware shows them
 * (port/panelgamma.h). */

/* Nearest: the sampler's own, which picks the source pixel under the
 * destination pixel's centre - floor((i + 0.5) / scale), the same pixel
 * brick.c's map picks. */
static const char *PLAIN_SHADER =
	"#version 300 es\n"
	"precision highp float;\n"
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_size;\n"
	"in vec2 v_src;\n"
	"out vec4 o_color;\n"
	"void main() { o_color = texture(u_tex, v_src / u_size); }\n";

/* Sharp-bilinear, brick.c's build_map in a shader: the bilinear weight,
 * steepened by the scale so the blend spans one destination pixel. Interior
 * pixels sample a texel centre exactly; only the pixel straddling a source
 * boundary mixes. At an integer factor every weight is 0 or 1, so it equals
 * nearest. The edges need no clamp of their own: CLAMP_TO_EDGE blends the
 * border texel with itself, which is brick.c's clamp. UNVERIFIED against
 * brick.c's output until a capture comparison. */
static const char *SHARP_SHADER =
	"#version 300 es\n"
	"precision highp float;\n"
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_size;\n"
	"uniform vec2 u_scale;\n"
	"in vec2 v_src;\n"
	"out vec4 o_color;\n"
	"void main() {\n"
	"	vec2 c = v_src - 0.5;\n"
	"	vec2 p = floor(c);\n"
	"	vec2 f = clamp((c - p - 0.5) * u_scale + 0.5, 0.0, 1.0);\n"
	"	o_color = texture(u_tex, (p + 0.5 + f) / u_size);\n"
	"}\n";

/* Sharp-Shimmerless, zadpos's box filter (public domain; THIRD-PARTY.md),
 * from its shaders_glsl/shaders/sharp-shimmerless.glsl at 2a74e7a, in GLES
 * 3.0 and in this pass's terms: v_src is in texels already, so the
 * destination pixel is (v_src - u_origin) * u_scale, measured from the
 * quad's first texel, and the original's per-vertex `pixel`, `scale` and
 * `invscale` are that, u_scale and u_inv. The rest is its main(): the
 * texels under the pixel's two edges, and where they differ, the point
 * between them whose bilinear weights are the areas each covers - one
 * texture read, the sampler doing the mix. Measured from v_src rather than
 * gl_FragCoord so that neither the panel's turn nor a GPU frame read
 * bottom-up changes the arithmetic: the box filter is the same both ways
 * along an axis.
 *
 * A pixel inside one texel lands on that texel's centre, so with LINEAR the
 * neighbour's weight is 0 and nothing bleeds past a DS screen's edge or a
 * GPU frame's last row, which sharp's comment below warns of. */
static const char *SHIMMERLESS_SHADER =
	"#version 300 es\n"
	"precision highp float;\n"
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_size;\n"
	"uniform vec2 u_scale;\n"
	"uniform vec2 u_inv;\n"
	"uniform vec2 u_origin;\n"
	"in vec2 v_src;\n"
	"out vec4 o_color;\n"
	"void main() {\n"
	"	vec2 px = (v_src - u_origin) * u_scale;\n"
	"	vec2 tl = floor(px), br = ceil(px);\n"
	"	vec2 t_tl = floor(u_inv * tl), t_br = floor(u_inv * br);\n"
	"	vec2 t = t_br + 0.5 - (1.0 - step(t_br, t_tl)) * (u_scale * t_br - tl);\n"
	"	o_color = texture(u_tex, (t + u_origin) / u_size);\n"
	"}\n";

/* What the shaders below work from, the same as Sharp-Shimmerless's, and
 * u_quad: the quad's source size in texels, which the CRT clamps its reads
 * to and warps over (quad_scale). */
#define QUAD_UNIFORMS \
	"uniform sampler2D u_tex;\n" \
	"uniform vec2 u_size;\n" \
	"uniform vec2 u_scale;\n" \
	"uniform vec2 u_inv;\n" \
	"uniform vec2 u_origin;\n" \
	"uniform vec2 u_quad;\n" \
	"in vec2 v_src;\n" \
	"out vec4 o_color;\n"

/* Sharp-Shimmerless with gaps: zadpos's sharp-shimmerless-grid and
 * -scanlines (CC0, the same repository at 2a74e7a), whose main() this is in
 * this pass's terms. Each game pixel is a rectangle with a dark band along
 * its right and bottom edges, RATIO of it wide and OPACITY dark, and a
 * screen pixel is what it covers, by area: the band stays the same width
 * however the picture is scaled, and nothing shimmers. One texture read.
 * `ratio` and `opacity` are vec2s, across and down: with nothing across it is
 * scanlines; with both, an LCD's grid. */
#define GRID_SHADER_SRC(ratio, alpha, light) \
	"#version 300 es\n" \
	"precision highp float;\n" \
	QUAD_UNIFORMS \
	"const vec2 RATIO = 1.0 - " ratio ";\n" \
	alpha \
	"void main() {\n" \
	"	vec2 px = (v_src - u_origin) * u_scale;\n" \
	"	vec2 tl = u_inv * floor(px), br = u_inv * ceil(px);\n" \
	"	vec2 t_tl = floor(tl), t_br = floor(br);\n" \
	"	vec2 ftl = fract(tl), fbr = fract(br);\n" \
	"	vec2 a_tl = mix(RATIO - ftl + ALPHA * (1.0 - RATIO), ALPHA * (1.0 - RATIO), step(RATIO, ftl));\n" \
	"	vec2 a_br = mix(fbr, RATIO + ALPHA * (fbr - RATIO), step(RATIO, fbr));\n" \
	"	vec2 w = mix(a_br / (a_tl + a_br), vec2(1.0), step(t_br, t_tl));\n" \
	"	vec2 f = (fbr + clamp(RATIO - ftl, 0.0, 1.0)) * u_scale;\n" \
	"	f = mix(f, RATIO * u_scale, step(RATIO, fbr));\n" \
	"	f = mix(f, clamp((RATIO - ftl) * u_scale, 0.0, 1.0), step(t_br, t_tl));\n" \
	"	f = ALPHA + (1.0 - ALPHA) * f;\n" \
	"	vec2 t = t_br - 0.5 + w;\n" \
	"	vec3 c = texture(u_tex, (t + u_origin) / u_size).rgb;\n" \
	light \
	"	o_color = vec4(f.x * f.y * c, 1.0);\n" \
	"}\n"
#define GRID_SHADER(ratio, opacity) \
	GRID_SHADER_SRC(ratio, "const vec2 ALPHA = 1.0 - " opacity ";\n", "")
/* Under 100% strength the band is that much as dark: OPACITY times
 * u_strength, 0 to 1. The band's width stays, so nothing shimmers at any
 * strength, and at 0 nothing is dark and the weights are the box filter's -
 * Sharp-Shimmerless, but for a pixel that starts inside a band, where the
 * original counts the band's whole width. Still one texture read. */
#define GRID_SHADER_SOFT(ratio, opacity) \
	GRID_SHADER_SRC(ratio, "uniform float u_strength;\n" \
	                "#define ALPHA (1.0 - " opacity " * u_strength)\n", "")

/* LCD No Backlight: the LCD's grid on a screen with nothing lit behind it,
 * as the Game Boy Color's and the first Game Boy Advance's were. Such a
 * screen shows a light pixel by letting light through to the reflective
 * layer behind it, so white is that layer's own dull, slightly green grey,
 * the dark pixels sit on it, and the grid shows over both. Here a pixel
 * goes toward BACKING by how light it is, by a soft curve, its luma
 * squared: black stays black, a dark grey moves a little, white becomes
 * BACKING.
 *
 * The liquid crystal sits a little above that layer, so a dark pixel casts
 * a shadow on it, down and to the right, which shows only where a light
 * pixel lets the layer through: the backing is darkened by SHADE times the
 * darkness of the source SHADOW texels up and to the left. Read as sharp
 * bilinear does, the point moved into its texel's middle but for the last
 * quarter on each side, so the shadow's edge is a soft quarter of a game
 * pixel at any scale with no blur. The grid's one read and this one: two a
 * pixel, where the Game Boy's three fill a fraction of the panel.
 *
 * Under 100% the backing's share, the shadow and the grid's dark all scale
 * by u_strength, so at 0 it is Sharp-Shimmerless. An idea from mattakins's
 * Pixel Transparency for RetroArch; this is ZyperOS's own code. */
#define UNLIT_BACKING \
	"const vec3 BACKING = vec3(0.78, 0.80, 0.71);\n" \
	"const vec3 LUMA = vec3(0.299, 0.587, 0.114);\n" \
	"const vec2 SHADOW = vec2(0.4, 0.4);\n" \
	"const float SHADE = 0.35;\n"
#define UNLIT_LIGHT(amount) \
	"	float l = dot(c, LUMA);\n" \
	"	vec2 q = v_src - u_origin - SHADOW - 0.5, qp = floor(q);\n" \
	"	q = qp + 0.5 + clamp((q - qp - 0.5) * 4.0 + 0.5, 0.0, 1.0);\n" \
	"	float s = 1.0 - dot(texture(u_tex, (q + u_origin) / u_size).rgb, LUMA);\n" \
	"	c = mix(c, BACKING * (1.0 - " amount " * SHADE * s), " amount " * l * l);\n"
#define LCD_UNLIT_SHADER_SRC \
	GRID_SHADER_SRC("vec2(0.3, 0.3)", UNLIT_BACKING "const vec2 ALPHA = 1.0 - vec2(0.3, 0.3);\n", \
	                UNLIT_LIGHT("1.0"))
#define LCD_UNLIT_SOFT_SHADER_SRC \
	GRID_SHADER_SRC("vec2(0.3, 0.3)", UNLIT_BACKING "uniform float u_strength;\n" \
	                "#define ALPHA (1.0 - vec2(0.3, 0.3) * u_strength)\n", \
	                UNLIT_LIGHT("u_strength"))

/* A handheld's LCD: the grid both ways, the original's defaults */
static const char *LCD_SHADER = GRID_SHADER("vec2(0.3, 0.3)", "vec2(0.3, 0.3)");
/* Scanlines: the band only down, the -scanlines variant's defaults */
static const char *SCANLINES_SHADER = GRID_SHADER("vec2(0.0, 0.3)", "vec2(0.0, 0.3)");
static const char *LCD_SOFT_SHADER = GRID_SHADER_SOFT("vec2(0.3, 0.3)", "vec2(0.3, 0.3)");
static const char *SCANLINES_SOFT_SHADER = GRID_SHADER_SOFT("vec2(0.0, 0.3)", "vec2(0.0, 0.3)");
static const char *LCD_UNLIT_SHADER = LCD_UNLIT_SHADER_SRC;
static const char *LCD_UNLIT_SOFT_SHADER = LCD_UNLIT_SOFT_SHADER_SRC;

/* A CRT: Timothy Lottes's CRTS (public domain; libretro glsl-shaders'
 * crt/shaders/crt-lottes-fast.glsl, its 20180120b), its 2-tap filter: two
 * reads a pixel, one on each of the two scanlines nearest, each bilinear at
 * a point Inigo Quilez's quintic moves so the pixel stays sharp across. The
 * scanlines are its windowed cosines, the phosphors its brighter aperture
 * grille (one channel of every three screen columns down to DARK), and its
 * tone curve gives back the light the two take. Its parameters' defaults,
 * fixed: thin 0.75, mask 0.5, and TONE is CrtsTone(1, 0, 0.75, 0.5) for
 * that grille.
 *
 * Changed for this GPU: gamma 2 (a square in, a square root out) where it
 * has 2.4 and pow() on every read, so the pass takes no pow() at all.
 * The mask goes by the screen column within the picture, so it stays
 * upright on the turned panel. With WARP, its curvature and corners, the
 * original's defaults: warp 0.02 across and 0.015 down (a 4:3 screen),
 * corner 3. Reads stay inside the quad, so nothing comes from the
 * other DS screen or past a GPU frame's last row. */
#define CRT_SHADER_SRC(warp, soft, mix) \
	"#version 300 es\n" \
	"precision highp float;\n" \
	warp \
	QUAD_UNIFORMS \
	soft \
	"const float THIN = 0.75;\n" \
	"const float DARK = 0.5;\n" \
	"const vec2 TONE = vec2(0.419207, 0.580793);\n" \
	"vec3 fetch(vec2 p) {\n" \
	"	vec3 c = texture(u_tex, (clamp(p, vec2(0.5), u_quad - 0.5) + u_origin) / u_size).rgb;\n" \
	"	return c * c;\n" \
	"}\n" \
	"void main() {\n" \
	"	vec2 px = (v_src - u_origin) * u_scale;\n" \
	"	vec2 pos = px * u_inv;\n" \
	"	float vin = 1.0;\n" \
	"#ifdef WARP\n" \
	"	vec2 q = px / (u_quad * u_scale) * 2.0 - 1.0;\n" \
	"	q *= vec2(1.0 + q.y * q.y * 0.02, 1.0 + q.x * q.x * 0.015);\n" \
	"	vin = (1.0 - (1.0 - clamp(q.x * q.x, 0.0, 1.0)) * (1.0 - clamp(q.y * q.y, 0.0, 1.0))) * 1.001;\n" \
	"	vin = clamp(u_quad.y - vin * u_quad.y, 0.0, 1.0);\n" \
	"	pos = (q + 1.0) * 0.5 * u_quad;\n" \
	"#endif\n" \
	"	float y0 = floor(pos.y - 0.5) + 0.5;\n" \
	"	float x = pos.x + 0.5, xi = floor(x), xf = x - xi;\n" \
	"	xf = xf * xf * xf * (xf * (xf * 6.0 - 15.0) + 10.0);\n" \
	"	vec2 p = vec2(xi + xf - 0.5, y0);\n" \
	"	vec3 a = fetch(p), b = fetch(p + vec2(0.0, 1.0));\n" \
	"	float off = pos.y - y0;\n" \
	"	float sa = cos(min(0.5, off * THIN) * 6.2831853) * 0.5 + 0.5;\n" \
	"	float sb = cos(min(0.5, (1.0 - off) * THIN) * 6.2831853) * 0.5 + 0.5;\n" \
	"	vec3 c = (a * sa + b * sb) * vin;\n" \
	"	float m = fract(px.x * (1.0 / 3.0));\n" \
	"	c *= m < 1.0 / 3.0 ? vec3(DARK, 1.0, 1.0) : m < 2.0 / 3.0 ? vec3(1.0, DARK, 1.0) : vec3(1.0, 1.0, DARK);\n" \
	"	float peak = max(1.0 / (256.0 * 65536.0), max(c.r, max(c.g, c.b)));\n" \
	"	c = c / peak * (peak / (peak * TONE.x + TONE.y));\n" \
	mix \
	"	o_color = vec4(sqrt(c), 1.0);\n" \
	"}\n"

/* Under 100% strength: the CRT mixed, in its linear light, toward its own
 * plain picture, the same two reads with the quintic down as well as across
 * and no scanlines, grille or tone; u_strength 0 to 1. The curve and the
 * round corners are the glass's shape, not its lines, and stay. */
#define CRT_SOFT \
	"uniform float u_strength;\n"
#define CRT_SOFT_MIX \
	"	float qy = off * off * off * (off * (off * 6.0 - 15.0) + 10.0);\n" \
	"	c = mix(mix(a, b, qy) * vin, c, u_strength);\n"

static const char *CRT_SHADER = CRT_SHADER_SRC("", "", "");
static const char *CRT_CURVED_SHADER = CRT_SHADER_SRC("#define WARP 1\n", "", "");
static const char *CRT_SOFT_SHADER = CRT_SHADER_SRC("", CRT_SOFT, CRT_SOFT_MIX);
static const char *CRT_CURVED_SOFT_SHADER = CRT_SHADER_SRC("#define WARP 1\n", CRT_SOFT, CRT_SOFT_MIX);

static const char *SOLID_SHADER =
	"#version 300 es\n"
	"precision mediump float;\n"
	"uniform vec4 u_color;\n"
	"out vec4 o_color;\n"
	"void main() { o_color = u_color; }\n";

static GLuint compile(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	GLint ok = 0;

	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(s, sizeof log, NULL, log);
		port_logf(DIATOM_LOG_ERROR, PORT_NAME ": shader: %s", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

/* A uniform's location, asked of the driver once a program and kept: by name
 * it is a string lookup in the driver, and the frame asked for ten or more
 * on every flip (u_tex, u_size, u_scale, the bar's and
 * the disc's). Emptied when a program is linked and when the context goes,
 * so a new program that gets an old one's id is asked again. The present
 * thread's GL alone calls this. */
#define ULOCS 64
static struct {
	GLuint prog;
	char   name[16];
	GLint  loc;
} g_uloc[ULOCS];
static int g_nuloc;

static GLint uloc(GLuint prog, const char *name)
{
	GLint loc;
	int i;

	for (i = 0; i < g_nuloc; i++)
		if (g_uloc[i].prog == prog && !strcmp(g_uloc[i].name, name)) return g_uloc[i].loc;
	loc = glGetUniformLocation(prog, name);
	if (g_nuloc < ULOCS && strlen(name) < sizeof g_uloc[0].name) {
		g_uloc[g_nuloc].prog = prog;
		snprintf(g_uloc[g_nuloc].name, sizeof g_uloc[g_nuloc].name, "%s", name);
		g_uloc[g_nuloc].loc = loc;
		g_nuloc++;
	}
	return loc;
}

static GLuint link_program(const char *fragment)
{
	GLuint vs = compile(GL_VERTEX_SHADER, VERTEX_SHADER);
	GLuint fs = compile(GL_FRAGMENT_SHADER, fragment);
	GLuint p;
	GLint ok = 0;

	if (!vs || !fs) return 0;
	g_nuloc = 0;
	p = glCreateProgram();
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glBindAttribLocation(p, 0, "a_pos");
	glBindAttribLocation(p, 1, "a_src");
	glLinkProgram(p);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		diatom_port_log(DIATOM_LOG_ERROR, PORT_NAME ": program did not link");
		glDeleteProgram(p);
		return 0;
	}
	return p;
}

/* A rect in SURFACE coordinates (landscape, y down) to clip space on the
 * target being drawn. On glass the panel is portrait and the picture turns
 * 90 degrees counter-clockwise, which on this panel ("Left Side Up") is the
 * right way up - checked by eye 2026-10-01 with a marker drawn top-left.
 * Landscape (x, y) lands on portrait (y, W - x). For a capture the target is
 * the surface itself and nothing turns. */
static void to_clip(float x, float y, bool turned, float *out)
{
	if (turned) {
		float px = y, py = (float)g_surface_w - x;
		out[0] = px / (float)g_panel_w * 2.0f - 1.0f;
		out[1] = 1.0f - py / (float)g_panel_h * 2.0f;
	} else {
		out[0] = x / (float)g_surface_w * 2.0f - 1.0f;
		out[1] = 1.0f - y / (float)g_surface_h * 2.0f;
	}
}

/* One textured or solid quad. `src` is in texels, matching the rect's corners:
 * top-left, top-right, bottom-left, bottom-right. */
static void draw_quad_from(float x, float y, float w, float h,
                           float sx, float sy, float sw, float sh, bool turned)
{
	float pos[8], src[8] = { sx, sy, sx + sw, sy, sx, sy + sh, sx + sw, sy + sh };

	to_clip(x,     y,     turned, &pos[0]);
	to_clip(x + w, y,     turned, &pos[2]);
	to_clip(x,     y + h, turned, &pos[4]);
	to_clip(x + w, y + h, turned, &pos[6]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, src);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void draw_quad(float x, float y, float w, float h,
                      float sw, float sh, bool turned)
{
	draw_quad_from(x, y, w, h, 0, 0, sw, sh, turned);
}

static void solid(float x, float y, float w, float h,
                  float r, float g, float b, float a, bool turned)
{
	glUseProgram(g_prog_solid);
	glUniform4f(uloc(g_prog_solid, "u_color"), r, g, b, a);
	draw_quad(x, y, w, h, 0, 0, turned);
}

/* Upload a mailbox frame into the frame texture. RGB565 goes up as is;
 * XRGB8888 goes up as bytes and the texture's swizzle puts B and R back and
 * pins alpha, so no shader needs to know which format it is drawing. */
static void upload_frame(const mailbox_frame *f)
{
	size_t bpp = (f->fmt == DIATOM_PIX_RGB565) ? 2 : 4;
	bool realloc_tex = f->w != g_tex_w || f->h != g_tex_h || f->fmt != g_tex_fmt;

	if (f->gpu) return;             /* already a texture: the core drew it */
	glBindTexture(GL_TEXTURE_2D, g_frame_tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(f->pitch / bpp));
	if (f->fmt == DIATOM_PIX_RGB565) {
		if (realloc_tex)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, f->w, f->h, 0,
			             GL_RGB, GL_UNSIGNED_SHORT_5_6_5, f->pixels);
		else
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, f->w, f->h,
			                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, f->pixels);
	} else {
		if (realloc_tex) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, f->w, f->h, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, f->pixels);
		} else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, f->w, f->h,
			                GL_RGBA, GL_UNSIGNED_BYTE, f->pixels);
		}
	}
	if (realloc_tex) {
		bool xrgb = f->fmt == DIATOM_PIX_XRGB8888;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, xrgb ? GL_BLUE : GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, xrgb ? GL_RED : GL_BLUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
		g_tex_w = f->w;
		g_tex_h = f->h;
		g_tex_fmt = f->fmt;
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

/* The level bar: the launcher's own pill, from the same code (osdbar.c, which
 * is ZyperOS's src/osdbar.c), so a volume press looks the same in a game as on
 * the shelf and over a port. Drawn into pixels at the surface's scale - 0.625
 * of the launcher's 1024 on the 640x480 panel - and redrawn only when the
 * look (the level, its mark moving, the boost past the top), the fade or the
 * theme moves; the slide is the quad's place. A change of the fade alone is
 * osdbar_render_look's conversion of what it last painted, not a paint. BGRA
 * with straight alpha, the overlay's own layout, so it goes up the overlay's
 * way. */
/* The look's colors as last handed to osdbar_colors, for draw_level_bar to
 * paint again when they move (render thread) */
static unsigned g_osd_colors_seen;

static void draw_level_bar(const osdbar_look *look, int theme, float alpha,
                           float rise, bool turned)
{
	static uint32_t px[OSDBAR_W * OSDBAR_H];
	static osdbar_look drawn;
	static bool drawn_any;
	static int drawn_theme, drawn_a, w, h;
	static unsigned drawn_gen;
	const float s = (float)g_surface_w / 1024.0f;
	int a = (int)(alpha * 255 + 0.5f);

	if (s <= 0 || s > 1) return;
	if (!drawn_any || memcmp(look, &drawn, sizeof drawn) ||
	    theme != drawn_theme || a != drawn_a || drawn_gen != g_osd_colors_seen) {
		osdbar_size(s, &w, &h);
		osdbar_render_look(px, w, h, w, s, theme, look, alpha, false);
		glBindTexture(GL_TEXTURE_2D, g_osdbar_tex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
		drawn = *look;
		drawn_any = true;
		drawn_theme = theme; drawn_a = a; drawn_gen = g_osd_colors_seen;
	}
	glBindTexture(GL_TEXTURE_2D, g_osdbar_tex);
	glUseProgram(g_prog_plain);
	glUniform1i(uloc(g_prog_plain, "u_tex"), 0);
	glUniform2f(uloc(g_prog_plain, "u_size"), (float)w, (float)h);
	draw_quad(roundf(OSDBAR_X * s), roundf((OSDBAR_Y - rise) * s), (float)w, (float)h,
	          (float)w, (float)h, turned);
}

/* On-Screen Stats (hud.c, ZyperOS's src/hud.c): painted into pixels at the
 * surface's scale, 0.625 on the 640x480 panel, only when its line, style or
 * the look's colors moved (twice a second at most), and drawn as one quad
 * where hud_place puts it on every frame while it is on. BGRA with straight
 * alpha, the level bar's way. */
static void draw_hud(bool repaint, const char *text, const hud_style *st,
                     const ds_theme *colors, bool turned)
{
	static uint32_t px[1024 * 128];
	static int w, h, x, y;
	const float s = (float)g_surface_w / 1024.0f;

	if (s <= 0 || s > 1) return;
	if (repaint) {
		hud_size(text, s, st, &w, &h);
		if (!w || w > 1024 || h > 128) { w = h = 0; return; }
		hud_render(px, w, h, w, s, text, st, colors, false);
		hud_place(st, s, w, h, g_surface_w, g_surface_h, &x, &y);
		glBindTexture(GL_TEXTURE_2D, g_hud_tex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
	}
	if (!w || !h) return;
	glBindTexture(GL_TEXTURE_2D, g_hud_tex);
	glUseProgram(g_prog_plain);
	glUniform1i(uloc(g_prog_plain, "u_tex"), 0);
	glUniform2f(uloc(g_prog_plain, "u_size"), (float)w, (float)h);
	draw_quad((float)x, (float)y, (float)w, (float)h, (float)w, (float)h, turned);
}

/* The low-battery disc: the one the launcher drew on the shelf until its
 * status corner replaced it (ZyperOS 87db5aa, which took out
 * draw_low_battery_dot), in its place and color, as horizontal spans so it
 * matches that disc to the pixel: radius 9, centered 34 px from the top and
 * right edges of the surface. */
static void draw_low_battery(bool turned)
{
	const int r = 9, cx = g_surface_w - 34, cy = 34;
	int dy;

	for (dy = -r; dy <= r; dy++) {
		const int v = r * r - dy * dy;
		int dx = 0;

		/* sqrt(v) rounded, as the launcher's span is: the largest dx with
		 * (2dx - 1)^2 <= 4v. */
		while ((2 * (dx + 1) - 1) * (2 * (dx + 1) - 1) <= 4 * v) dx++;
		solid((float)(cx - dx), (float)(cy + dy), (float)(2 * dx + 1), 1,
		      224 / 255.f, 72 / 255.f, 72 / 255.f, 1, turned);
	}
}

/* Each filter's shader, in diatom_filter's order; nearest is the sampler's
 * own (PLAIN_SHADER), the rest are shaders over a LINEAR sampler. A shader
 * to come is a row here and in diatom_filters. */
static const char *const *const FILTER_SHADER[DIATOM_FILTER_COUNT] = {
	[DIATOM_FILTER_NEAREST]     = &PLAIN_SHADER,
	[DIATOM_FILTER_SHARP]       = &SHARP_SHADER,
	[DIATOM_FILTER_SHIMMERLESS] = &SHIMMERLESS_SHADER,
	[DIATOM_FILTER_CRT]         = &CRT_SHADER,
	[DIATOM_FILTER_CRT_CURVED]  = &CRT_CURVED_SHADER,
	[DIATOM_FILTER_SCANLINES]   = &SCANLINES_SHADER,
	[DIATOM_FILTER_LCD]         = &LCD_SHADER,
	[DIATOM_FILTER_LCD_UNLIT]   = &LCD_UNLIT_SHADER,
};
/* And under 100% strength, for the filters that have an effect to weaken */
static const char *const *const SOFT_SHADER[DIATOM_FILTER_COUNT] = {
	[DIATOM_FILTER_CRT]         = &CRT_SOFT_SHADER,
	[DIATOM_FILTER_CRT_CURVED]  = &CRT_CURVED_SOFT_SHADER,
	[DIATOM_FILTER_SCANLINES]   = &SCANLINES_SOFT_SHADER,
	[DIATOM_FILTER_LCD]         = &LCD_SOFT_SHADER,
	[DIATOM_FILTER_LCD_UNLIT]   = &LCD_UNLIT_SOFT_SHADER,
};

/* The final pass's program for a filter, linked the first time it is asked
 * for rather than at start: Diatom starts with the device, and six programs
 * nobody may choose are time spent then, where one link as the game the
 * player chose it for continues is a frame. Nearest's, the one the overlay
 * and the level bar use too, at start (gl_start). A link that fails draws
 * nearest, and says so once. */
static GLuint filter_prog(diatom_filter f, int strength, bool *soft)
{
	*soft = false;
	if (f <= DIATOM_FILTER_NEAREST || f >= DIATOM_FILTER_COUNT) return g_prog_plain;
	/* Under 100%: the soft program, linked the same way. One that fails
	 * draws the filter at full, which is closer than nearest. */
	if (strength < 100 && SOFT_SHADER[f]) {
		if (!g_prog_soft[f] && !g_soft_failed[f]) {
			g_prog_soft[f] = link_program(*SOFT_SHADER[f]);
			g_soft_failed[f] = !g_prog_soft[f];
			if (g_soft_failed[f])
				port_logf(DIATOM_LOG_ERROR, PORT_NAME ": filter %d's strength did not link; full", (int)f);
		}
		if (g_prog_soft[f]) {
			*soft = true;
			return g_prog_soft[f];
		}
	}
	if (!g_prog_filter[f] && !g_prog_failed[f]) {
		g_prog_filter[f] = link_program(*FILTER_SHADER[f]);
		g_prog_failed[f] = !g_prog_filter[f];
		if (g_prog_failed[f])
			port_logf(DIATOM_LOG_ERROR, PORT_NAME ": filter %d did not link; nearest", (int)f);
	}
	return g_prog_filter[f] ? g_prog_filter[f] : g_prog_plain;
}

/* A quad's scale, for the programs that read one: destination pixels per
 * source texel on each axis; and for the shaders that work in destination
 * pixels (Sharp-Shimmerless, the grid, the scanlines, the CRT) its inverse,
 * the quad's first texel, the low corner of its source whichever way the
 * rows are read (sh < 0: a GPU frame, bottom-up), and its size in texels.
 * Nearest reads none of it and is not asked; a uniform a program does not
 * have is location -1, which GL ignores. */
static void quad_scale(diatom_filter filter, GLuint prog, float dw, float dh,
                       float sx, float sy, float sw, float sh)
{
	sh = fabsf(sh);
	if (prog == g_prog_plain || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
	glUniform2f(uloc(prog, "u_scale"), dw / sw, dh / sh);
	if (filter == DIATOM_FILTER_SHARP) return;
	glUniform2f(uloc(prog, "u_inv"), sw / dw, sh / dh);
	glUniform2f(uloc(prog, "u_origin"), sx, sy);
	glUniform2f(uloc(prog, "u_quad"), sw, sh);
}

/* Draw one frame and whatever is over it, onto the current target. */
static void draw_scene(const mailbox_frame *f, bool border, bool overlay_live, bool osd_live,
                       const osdbar_look *osd_look, int osd_theme,
                       float osd_alpha, float osd_rise, bool lowbat,
                       bool hud_live, bool hud_repaint, const char *hud_text_now,
                       const hud_style *hud_st, const ds_theme *hud_colors, bool turned)
{
	bool   soft;
	GLuint prog = filter_prog(f->filter, f->strength, &soft);
	GLint  filter = prog == g_prog_plain ? GL_NEAREST : GL_LINEAR;
	/* A GPU frame is GL's way up, its first row at the bottom of a texture
	 * larger than it: sampled from row h down to row 0, so the same quads
	 * draw it upright. Unless its core draws the first row at the top. */
	bool   flip = f->gpu && !f->top_down;
	float  top  = flip ? (float)f->h : 0.0f;
	float  down = flip ? -1.0f : 1.0f;

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	/* The Border (diatom_port_border), stretched over the whole surface
	 * under the game: its texture uploaded once, so this is a quad drawn
	 * where the clear was. Opaque, its alpha swizzled to 1, and no blending
	 * is on yet. */
	/* None where the picture covers the surface: nothing to show it in */
	if (border && g_bd_tex_w > 0 && g_bd_tex_h > 0 && !f->nplanes &&
	    (f->dst.x > 0 || f->dst.y > 0 || f->dst.x + f->dst.w < g_surface_w ||
	     f->dst.y + f->dst.h < g_surface_h)) {
		glBindTexture(GL_TEXTURE_2D, g_border_tex);
		glUseProgram(g_prog_plain);
		glUniform1i(uloc(g_prog_plain, "u_tex"), 0);
		glUniform2f(uloc(g_prog_plain, "u_size"), (float)g_bd_tex_w, (float)g_bd_tex_h);
		draw_quad(0.0f, 0.0f, (float)g_surface_w, (float)g_surface_h,
		          (float)g_bd_tex_w, (float)g_bd_tex_h, turned);
	}

	glBindTexture(GL_TEXTURE_2D, f->gpu ? f->gpu_tex : g_frame_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	/* Whatever alpha the core left in it, the panel gets 1 (see below) */
	if (f->gpu) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
	glUseProgram(prog);
	glUniform1i(uloc(prog, "u_tex"), 0);
	glUniform2f(uloc(prog, "u_size"),
	            (float)(f->gpu ? f->tex_w : f->w), (float)(f->gpu ? f->tex_h : f->h));
	if (soft) glUniform1f(uloc(prog, "u_strength"), (float)f->strength / 100.0f);
	if (f->nplanes) {
		/* A DS frame's two screens, each its own quad out of the one
		 * texture (diatom_port_planes). Sharp's steepness is per quad: the
		 * two screens of a focus layout are at 2x and 0.5x. The sampler
		 * clamps only at the texture's edge, so with sharp a screen's last
		 * row can take a little of the other screen's first, where the two
		 * meet in the middle of the texture; nearest never reaches past its
		 * own texel, and Sharp-Shimmerless reads a screen's last texel at
		 * its centre (SHIMMERLESS_SHADER). */
		int i;

		for (i = 0; i < f->nplanes; i++) {
			const diatom_plane *p = &f->planes[i];
			const float sy = top + down * (float)p->src.y;

			quad_scale(f->filter, prog, (float)p->dst.w, (float)p->dst.h, (float)p->src.x,
			           flip ? sy - (float)p->src.h : sy,
			           (float)p->src.w, (float)p->src.h);
			draw_quad_from((float)p->dst.x, (float)p->dst.y,
			               (float)p->dst.w, (float)p->dst.h,
			               (float)p->src.x, top + down * (float)p->src.y,
			               (float)p->src.w, down * (float)p->src.h, turned);
		}
	} else {
		quad_scale(f->filter, prog, (float)f->dst.w, (float)f->dst.h,
		           0.0f, 0.0f, (float)f->w, (float)f->h);
		/* dst may reach past the surface (fill, overscale): GL clips it. */
		draw_quad_from((float)f->dst.x, (float)f->dst.y, (float)f->dst.w, (float)f->dst.h,
		               0.0f, top, (float)f->w, down * (float)f->h, turned);
	}

	/* Alpha in the target stays 1 whatever is blended over it: the scanout
	 * format carries alpha, and this display controller is not trusted to
	 * ignore it (the Brick's composited per pixel - see brick.c, g_opaque). */
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
	/* On-Screen Stats under the rest: an achievement's notice draws over a
	 * line in a bottom corner, the level bar and the disc are clear of it */
	if (hud_live)
		draw_hud(hud_repaint, hud_text_now, hud_st, hud_colors, turned);
	if (overlay_live) {
		/* The launcher draws a notice in its 1024-unit screen, as the level
		 * bar is drawn (draw_level_bar), so it is scaled to the surface the
		 * same way: 640 wide on this panel. Drawn at the surface's size
		 * instead, a notice over 640 units wide - most achievements, "Getting
		 * hot" - was refused, and a narrower one came out 1.6 times too
		 * large (2026-11-06). */
		const float s = ov_scale();
		float ow = roundf(g_ov_w * s), oh = roundf(g_ov_h * s);
		float x0 = roundf((g_surface_w - ow) / 2.0f);
		float y0 = roundf(g_surface_h - oh - g_surface_h / 24.0f);

		glBindTexture(GL_TEXTURE_2D, g_overlay_tex);
		glUseProgram(g_prog_plain);
		glUniform1i(uloc(g_prog_plain, "u_tex"), 0);
		glUniform2f(uloc(g_prog_plain, "u_size"),
		            (float)g_ov_w, (float)g_ov_h);
		draw_quad(x0, y0, ow, oh, (float)g_ov_w, (float)g_ov_h, turned);
	}
	if (osd_live && osd_look)
		draw_level_bar(osd_look, osd_theme, osd_alpha, osd_rise, turned);
	if (lowbat)
		draw_low_battery(turned);
	glDisable(GL_BLEND);
}

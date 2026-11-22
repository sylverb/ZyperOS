/* The old screens' GLSL: every program Diatom's GPU ports draw a frame with
 * (the plain copy, Sharp, Sharp-Shimmerless, the CRT, CRT Curved, the
 * scanlines, the LCD's grid and LCD No Backlight, each under 100% too), and
 * which filter takes which, in diatom_filter's order. Cut from
 * port/gles_draw.h, which includes it where they were, so the Pixel 2's and
 * the Brick's programs are the same; and ZyperOS's PICO-8 shim includes it
 * too (ZyperOS's src/p8fx/shim.c), so your PICO-8 is drawn with the very
 * same shaders as a game in Diatom.
 *
 * Not a header of declarations: static definitions of strings and two
 * tables, for each file that includes it, with diatom_port.h's
 * diatom_filter before it. */
#ifndef DIATOM_GLES_SHADERS_H
#define DIATOM_GLES_SHADERS_H
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

#endif

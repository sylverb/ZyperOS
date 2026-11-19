/* Why a core would not load (corefail.h). */
#include "corefail.h"

#include <stdio.h>
#include <string.h>

#define EM_X86_64  62
#define EM_AARCH64 183

unsigned corefail_machine(void)
{
#if defined(__linux__) && defined(__aarch64__)
	return EM_AARCH64;
#elif defined(__linux__) && defined(__x86_64__)
	return EM_X86_64;
#else
	return 0;
#endif
}

const char *corefail_word(corefail_why why)
{
	switch (why) {
	case COREFAIL_MACHINE: return "machine";
	case COREFAIL_LIBRARY: return "library";
	case COREFAIL_VERSION: return "version";
	case COREFAIL_SYMBOL:  return "symbol";
	default:               return "other";
	}
}

/* What the file's first bytes say: 1 an ELF of this build's kind (64-bit,
 * little-endian, `want`'s machine), 0 an ELF of another, -1 not an ELF, -2
 * not readable. With want 0 any ELF is of another kind. */
static int elf_kind(const char *path, unsigned want)
{
	unsigned char h[20];
	size_t n;
	FILE *f = fopen(path, "rb");

	if (!f) return -2;
	n = fread(h, 1, sizeof h, f);
	fclose(f);
	if (n < sizeof h || h[0] != 0x7f || h[1] != 'E' || h[2] != 'L' || h[3] != 'F') return -1;
	if (want == 0) return 0;
	if (h[4] != 2 || h[5] != 1) return 0;               /* ELFCLASS64, ELFDATA2LSB */
	return (unsigned)(h[18] | (h[19] << 8)) == want ? 1 : 0;
}

/* [b, e) as a name: printable ASCII only (a tab or a line end would split the
 * line Diatom sends, and the launcher draws what it reads), no more than the
 * buffer holds */
static void put_name(char *out, size_t cap, const char *b, const char *e)
{
	size_t k = 0;

	for (; b < e && *b && k + 1 < cap; b++)
		out[k++] = (*b >= 0x20 && *b < 0x7f) ? *b : '?';
	out[k] = '\0';
}

/* [b, e) without the folders before its last slash */
static const char *file_of(const char *b, const char *e)
{
	const char *p;

	for (p = e; p > b && p[-1] != '/'; p--)
		;
	return p;
}

/* Whether [b, e) is the core's own file's name, however the folders were
 * said: glibc names the core by the path it was given when it is the core
 * that could not be opened */
static int is_core(const char *core, const char *b, const char *e)
{
	const char *c = file_of(core, core + strlen(core));
	size_t n = strlen(c);

	b = file_of(b, e);
	return (size_t)(e - b) == n && !strncmp(b, c, n);
}

/* The end of the line `s` is on */
static const char *eol(const char *s)
{
	const char *nl = strchr(s, '\n');

	return nl ? nl : s + strlen(s);
}

void corefail_classify(corefail *f, const char *core, const char *err, unsigned want)
{
	const char *p, *q, *e;
	int kind;

	memset(f, 0, sizeof *f);
	f->why = COREFAIL_OTHER;
	if (!core) core = "";
	if (!err) err = "";

	/* The header first: glibc's text for a file of another machine is
	 * ENOENT, the name of a file that is there */
	kind = elf_kind(core, want);
	if (kind == 0 || (kind == -1 && want != 0)) {
		f->why = COREFAIL_MACHINE;
		return;
	}

	/* glibc: "<file>: version `V' not found (required by <core>)". The
	 * file is the library, there and too old. */
	if ((p = strstr(err, ": version `")) != NULL && (q = strchr(p + 11, '\'')) != NULL &&
	    strstr(q, "not found")) {
		put_name(f->name, sizeof f->name, file_of(err, p), p);
		put_name(f->version, sizeof f->version, p + 11, q);
		f->why = COREFAIL_VERSION;
		return;
	}

	/* A function nothing has. glibc: "<core>: undefined symbol: S", maybe
	 * ", version V" after it; musl: "Error relocating <core>: S: symbol not
	 * found"; macOS: "symbol not found in flat namespace '_S'". */
	if ((p = strstr(err, "undefined symbol: ")) != NULL) {
		p += 18;
		e = p + strcspn(p, ",\n");
		put_name(f->name, sizeof f->name, p, e);
		f->why = COREFAIL_SYMBOL;
		return;
	}
	if (strstr(err, "Error relocating ") && (q = strstr(err, ": symbol not found")) != NULL) {
		for (p = q; p > err && !(p[-1] == ' ' && p - 1 > err && p[-2] == ':'); p--)
			;
		put_name(f->name, sizeof f->name, p, q);
		f->why = COREFAIL_SYMBOL;
		return;
	}
	if ((p = strstr(err, "symbol not found in flat namespace '")) != NULL) {
		p += 36;
		if (*p == '_') p++;
		q = strchr(p, '\'');
		put_name(f->name, sizeof f->name, p, q ? q : p + strlen(p));
		f->why = COREFAIL_SYMBOL;
		return;
	}

	/* A library that is not there. glibc: "<name>: cannot open shared object
	 * file: No such file or directory"; musl: "Error loading shared library
	 * <name>: No such file or directory (needed by <core>)"; macOS: "Library
	 * not loaded: <name>". The core's own name is the core that could not be
	 * opened, which is not a library it needs. */
	p = e = NULL;
	if ((q = strstr(err, ": cannot open shared object file")) != NULL && q < eol(err)) {
		p = err;
		e = q;
	} else if ((q = strstr(err, "Error loading shared library ")) != NULL) {
		p = q + 29;
		e = strstr(p, ": ");
		if (!e || e > eol(p)) e = eol(p);
	} else if ((q = strstr(err, "Library not loaded: ")) != NULL) {
		p = q + 20;
		e = eol(p);
	}
	if (p && e > p && !is_core(core, p, e)) {
		put_name(f->name, sizeof f->name, file_of(p, e), e);
		f->why = COREFAIL_LIBRARY;
	}
}

/* src/corefail.c: why a core would not load, from the file's ELF header and
 * dlerror's text, as glibc, musl and macOS say it (the stub core and the real
 * dlopen, end to end, are test/corefail.py).
 *
 * Offline and instant: it links src/corefail.c and nothing else. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "corefail.h"

static int g_fail, g_pass;

static void ck(int cond, const char *what)
{
	if (cond) { g_pass++; return; }
	g_fail++;
	printf("  FAIL %s\n", what);
}

#define EM_AARCH64 183
#define EM_X86_64  62
#define EM_ARM     40

static char dir[256];

/* A file of `n` bytes that begin as an ELF header says: class (1 or 2), byte
 * order (1 little, 2 big) and machine, written in that byte order. Returns
 * its path, in a static buffer per call index. */
static const char *elf(int slot, int cls, int data, unsigned machine)
{
	static char path[4][300];
	unsigned char h[64];
	FILE *f;

	memset(h, 0, sizeof h);
	h[0] = 0x7f; h[1] = 'E'; h[2] = 'L'; h[3] = 'F';
	h[4] = (unsigned char)cls;
	h[5] = (unsigned char)data;
	h[6] = 1;
	if (data == 2) { h[18] = (unsigned char)(machine >> 8); h[19] = (unsigned char)machine; }
	else           { h[18] = (unsigned char)machine; h[19] = (unsigned char)(machine >> 8); }
	snprintf(path[slot], sizeof path[slot], "%s/core%d_libretro.so", dir, slot);
	f = fopen(path[slot], "wb");
	if (!f) { perror(path[slot]); exit(2); }
	fwrite(h, 1, sizeof h, f);
	fclose(f);
	return path[slot];
}

static const char *text(const char *body)
{
	static char path[300];
	FILE *f;

	snprintf(path, sizeof path, "%s/text_libretro.so", dir);
	f = fopen(path, "wb");
	if (!f) { perror(path); exit(2); }
	fputs(body, f);
	fclose(f);
	return path;
}

static void say(const char *err_fmt, const char *core, char *out, size_t n)
{
	/* dlerror's texts carry the core's path; the table writes it as %s */
	snprintf(out, n, err_fmt, core);
}

int main(void)
{
	char tmpl[] = "/tmp/corefail-test-XXXXXX";
	char err[512];
	corefail f;
	const char *good, *x86, *arm32, *be;

	if (!mkdtemp(tmpl)) { perror("mkdtemp"); return 2; }
	snprintf(dir, sizeof dir, "%s", tmpl);

	good = elf(0, 2, 1, EM_AARCH64);
	x86  = elf(1, 2, 1, EM_X86_64);
	arm32 = elf(2, 1, 1, EM_ARM);
	be   = elf(3, 2, 2, EM_AARCH64);

	/* ---- another machine: the header, whatever dlerror said --------------- */
	/* glibc on an aarch64 Linux for an x86-64 build: the core's own name and
	 * ENOENT, for a file that is there (Davistoa's, 2026-11-18) */
	say("%s: cannot open shared object file: No such file or directory", x86, err, sizeof err);
	corefail_classify(&f, x86, err, EM_AARCH64);
	ck(f.why == COREFAIL_MACHINE, "an x86-64 core on an aarch64 build, glibc's ENOENT: another machine");
	corefail_classify(&f, arm32, "wrong ELF class: ELFCLASS32", EM_AARCH64);
	ck(f.why == COREFAIL_MACHINE, "a 32-bit ARM core: another machine");
	corefail_classify(&f, be, NULL, EM_AARCH64);
	ck(f.why == COREFAIL_MACHINE, "a big-endian core: another machine");
	corefail_classify(&f, text("#!/bin/sh\n"), "file too short", EM_AARCH64);
	ck(f.why == COREFAIL_MACHINE, "a file that is not an ELF at all: not a build for this system");
	corefail_classify(&f, x86, "", EM_X86_64);
	ck(f.why == COREFAIL_OTHER, "an x86-64 core on an x86-64 build, nothing said: not another machine");
	corefail_classify(&f, x86, "libx.so.1: cannot open shared object file: No such file or directory", 0);
	ck(f.why == COREFAIL_MACHINE, "no machine known (a Mac): any ELF is of another");
	corefail_classify(&f, text("not a core"),
	                  "dlopen(x, 0x0002): tried: 'x' (not a mach-o file)", 0);
	ck(f.why == COREFAIL_OTHER, "no machine known, not an ELF: dlerror's text decides");

	/* ---- a library it needs ----------------------------------------------- */
	corefail_classify(&f, good, "libfoo.so.2: cannot open shared object file: No such file or directory",
	                  EM_AARCH64);
	ck(f.why == COREFAIL_LIBRARY && !strcmp(f.name, "libfoo.so.2"), "glibc: libfoo.so.2 is missing");
	corefail_classify(&f, good,
	                  "/usr/lib/aarch64-linux-gnu/libfoo.so.2: cannot open shared object file: No such file or directory",
	                  EM_AARCH64);
	ck(f.why == COREFAIL_LIBRARY && !strcmp(f.name, "libfoo.so.2"), "glibc with a path: its file's name");
	say("Error loading shared library libbar.so.1: No such file or directory (needed by %s)", good,
	    err, sizeof err);
	corefail_classify(&f, good, err, EM_AARCH64);
	ck(f.why == COREFAIL_LIBRARY && !strcmp(f.name, "libbar.so.1"), "musl: libbar.so.1 is missing");
	corefail_classify(&f, text("x"),
	                  "dlopen(/x/c.so, 0x0002): Library not loaded: @rpath/libfoo.dylib\n"
	                  "  Referenced from: <ABC> /x/c.so\n  Reason: tried: '/x/libfoo.dylib' (no such file)",
	                  0);
	ck(f.why == COREFAIL_LIBRARY && !strcmp(f.name, "libfoo.dylib"), "macOS: libfoo.dylib, its path and the rest dropped");
	/* The core itself, not a library: it could not be opened (a link, a deleted file) */
	say("%s: cannot open shared object file: No such file or directory", good, err, sizeof err);
	corefail_classify(&f, good, err, EM_AARCH64);
	ck(f.why == COREFAIL_OTHER, "the core's own name in ENOENT, an aarch64 file: not a library");
	corefail_classify(&f, "/nowhere/core_libretro.so",
	                  "/nowhere/core_libretro.so: cannot open shared object file: No such file or directory",
	                  EM_AARCH64);
	ck(f.why == COREFAIL_OTHER, "a core that is not there: not a library");
	corefail_classify(&f, "/nowhere/core_libretro.so",
	                  "libfoo.so.2: cannot open shared object file: No such file or directory", EM_AARCH64);
	ck(f.why == COREFAIL_LIBRARY, "a core not readable and a library named: the library");

	/* ---- a library that is too old ---------------------------------------- */
	say("/lib/aarch64-linux-gnu/libstdc++.so.6: version `GLIBCXX_3.4.30' not found (required by %s)", good,
	    err, sizeof err);
	corefail_classify(&f, good, err, EM_AARCH64);
	ck(f.why == COREFAIL_VERSION && !strcmp(f.name, "libstdc++.so.6") && !strcmp(f.version, "GLIBCXX_3.4.30"),
	   "glibc: libstdc++.so.6 lacks GLIBCXX_3.4.30");
	corefail_classify(&f, good,
	                  "/lib/libc.so.6: version `GLIBC_2.38' not found (required by /x/c.so)", EM_AARCH64);
	ck(f.why == COREFAIL_VERSION && !strcmp(f.name, "libc.so.6") && !strcmp(f.version, "GLIBC_2.38"),
	   "glibc: libc.so.6 lacks GLIBC_2.38");

	/* ---- a function nothing has ------------------------------------------- */
	say("%s: undefined symbol: _ZN5bsnes3fooEv", good, err, sizeof err);
	corefail_classify(&f, good, err, EM_AARCH64);
	ck(f.why == COREFAIL_SYMBOL && !strcmp(f.name, "_ZN5bsnes3fooEv"), "glibc: undefined symbol");
	say("%s: undefined symbol: foo, version GLIBC_2.34", good, err, sizeof err);
	corefail_classify(&f, good, err, EM_AARCH64);
	ck(f.why == COREFAIL_SYMBOL && !strcmp(f.name, "foo"), "glibc with a version: the name alone");
	say("Error relocating %s: bar: symbol not found", good, err, sizeof err);
	corefail_classify(&f, good, err, EM_AARCH64);
	ck(f.why == COREFAIL_SYMBOL && !strcmp(f.name, "bar"), "musl: symbol not found");
	corefail_classify(&f, text("x"), "dlopen(/x/c.so, 0x0002): symbol not found in flat namespace '_baz'", 0);
	ck(f.why == COREFAIL_SYMBOL && !strcmp(f.name, "baz"), "macOS: flat namespace, the underscore dropped");

	/* ---- nothing known, and names that are not to be trusted -------------- */
	corefail_classify(&f, good, NULL, EM_AARCH64);
	ck(f.why == COREFAIL_OTHER && !f.name[0], "no dlerror: other");
	corefail_classify(&f, good, "something no C library says", EM_AARCH64);
	ck(f.why == COREFAIL_OTHER, "an unknown text: other");
	corefail_classify(&f, good, "lib\tfoo\rbar.so.1: cannot open shared object file: No such file or directory",
	                  EM_AARCH64);
	ck(f.why == COREFAIL_LIBRARY && !strpbrk(f.name, "\t\n\r"),
	   "a name with a tab or a carriage return in it: neither stays (the line is tab-separated)");
	{
		char big[1024];

		memset(big, 'a', sizeof big);
		memcpy(big + 900, ": undefined symbol: x", 21);
		big[sizeof big - 1] = '\0';
		corefail_classify(&f, good, big, EM_AARCH64);
		ck(strlen(f.name) < sizeof f.name && strlen(f.version) < sizeof f.version, "a long text: names stay in their buffers");
		memset(big, 'b', sizeof big);
		big[sizeof big - 1] = '\0';
		memcpy(big + 900, ": cannot open shared object file", 32);
		corefail_classify(&f, good, big, EM_AARCH64);
		ck(strlen(f.name) < sizeof f.name, "a name of a thousand characters: cut");
	}

	/* ---- words ------------------------------------------------------------ */
	ck(!strcmp(corefail_word(COREFAIL_MACHINE), "machine") && !strcmp(corefail_word(COREFAIL_LIBRARY), "library") &&
	   !strcmp(corefail_word(COREFAIL_VERSION), "version") && !strcmp(corefail_word(COREFAIL_SYMBOL), "symbol") &&
	   !strcmp(corefail_word(COREFAIL_OTHER), "other"), "the words the line carries");
#if defined(__linux__) && defined(__aarch64__)
	ck(corefail_machine() == EM_AARCH64, "this build wants aarch64 cores");
#elif defined(__linux__) && defined(__x86_64__)
	ck(corefail_machine() == EM_X86_64, "this build wants x86-64 cores");
#else
	ck(corefail_machine() == 0, "this build wants no ELF cores");
#endif

	{
		char cmd[300];

		snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
		if (system(cmd) != 0) { /* a scratch folder left in /tmp */ }
	}
	if (g_fail) {
		printf("corefail: %d failed, %d passed\n", g_fail, g_pass);
		return 1;
	}
	printf("ok: corefail, %d checks - the ELF header and dlerror's text, as glibc, musl and macOS say them\n", g_pass);
	return 0;
}

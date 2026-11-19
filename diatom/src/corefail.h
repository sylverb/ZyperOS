/* Why a core would not load, in words the launcher can put on the screen.
 *
 * A core a person added (bsnes, MAME 2010, Davistoa on Discord, 2026-11-18)
 * that dlopen refuses sent the game straight back to the shelf with nothing
 * said: the log had "dlopen <path>: <dlerror()>" and the screen had nothing.
 * The text of dlerror is the C library's, one way in glibc, another in musl,
 * another in macOS, and for the commonest cause it is wrong:
 *
 *   An x86-64 build on an aarch64 Linux: glibc skips a file of another machine
 *   and ends "<path>: cannot open shared object file: No such file or
 *   directory", the file's own name and ENOENT, for a file that is there.
 *   So the file's ELF header says which: its class, its byte order and its
 *   machine (e_machine). A core that is not a 64-bit little-endian ELF of this
 *   build's machine is "not a build for this system", whatever dlerror said.
 *
 *   A library it needs that the system lacks (an Arch Linux ARM build asking
 *   for libfoo.so.2): glibc "libfoo.so.2: cannot open shared object file: No
 *   such file or directory", musl "Error loading shared library libfoo.so.2:
 *   No such file or directory (needed by <path>)", macOS "Library not loaded:
 *   libfoo.dylib". The name is the library's, and when it is the core's own
 *   file's, the core could not be opened at all, which is not a library.
 *
 *   A library that is there and too old: glibc "/lib/libstdc++.so.6: version
 *   `GLIBCXX_3.4.30' not found (required by <path>)".
 *
 *   A function it needs that nothing has: glibc "<path>: undefined symbol:
 *   foo", musl "Error relocating <path>: foo: symbol not found", macOS
 *   "symbol not found in flat namespace '_foo'". dlopen is RTLD_NOW here, so
 *   a core that calls what is missing fails at the load, not in a game.
 *
 * Anything else is COREFAIL_OTHER, and the launcher says only that the core
 * could not be loaded. The full dlerror stays in the log (core.c).
 *
 * The line Diatom sends is the existing refusal with these fields added (all
 * unknown keys are ignored by an older launcher, so proto stays 14):
 *
 *   ERROR code=core_load msg=<path> why=<word> name=<it> version=<it>
 *
 * why is corefail_word(): machine, library, version, symbol or other. */
#ifndef DIATOM_COREFAIL_H
#define DIATOM_COREFAIL_H

#include <stddef.h>

typedef enum {
	COREFAIL_OTHER,     /* dlerror says something this does not know, or nothing */
	COREFAIL_MACHINE,   /* not a build for this system: the ELF header says so */
	COREFAIL_LIBRARY,   /* a library it needs is not there; name is its file's name */
	COREFAIL_VERSION,   /* a library is there and too old; name its file, version the one asked for */
	COREFAIL_SYMBOL,    /* a function it needs is nowhere; name is the symbol */
} corefail_why;

typedef struct {
	corefail_why why;
	char name[128];       /* the library's or the symbol's name, printable ASCII; "" for none */
	char version[64];     /* COREFAIL_VERSION's: "GLIBCXX_3.4.30" */
} corefail;

/* The ELF machine of the cores this build loads (e_machine): 183 on a 64-bit
 * ARM Linux, 62 on an x86-64 one, 0 where cores are not ELF files (a Mac's
 * desktop build). */
unsigned corefail_machine(void);

/* Say why `core` (the file's path) failed, given dlerror's text (NULL or ""
 * when there was none) and the machine the build wants (corefail_machine; a
 * test says another). With want 0 a file that is an ELF at all is of another
 * machine and one that is not is left to dlerror's text. Never fails. */
void corefail_classify(corefail *f, const char *core, const char *err, unsigned want);

/* The word the line carries: "machine", "library", "version", "symbol",
 * "other" */
const char *corefail_word(corefail_why why);

#endif /* DIATOM_COREFAIL_H */

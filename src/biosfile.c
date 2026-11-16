/* SPDX-License-Identifier: GPL-3.0-only */
#include "biosfile.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "i18n.h"

/* A name is there when it is a file that can be read: what the launcher
 * asked before it knew sizes (access R_OK), less a folder of the name */
static bool bios_there(const char *path, struct stat *st)
{
	return stat(path, st) == 0 && S_ISREG(st->st_mode) && access(path, R_OK) == 0;
}

static bool size_good(const unsigned *want, off_t bytes)
{
	int i;

	for (i = 0; i < CFG_BIOS_SIZES && want[i]; i++)
		if ((unsigned long long)bytes == want[i]) return true;
	return false;
}

bios_state bios_check(const system_cfg *s, const char *dir, bios_result *r)
{
	const char *p = s->disc_bios;
	int k;

	memset(r, 0, sizeof *r);
	if (!*p) return r->state = BIOS_OK;
	for (k = 0; *p; k++) {
		size_t n = strcspn(p, ",");
		char path[CFG_STR * 3];
		struct stat st;

		snprintf(path, sizeof path, "%s/%.*s", dir, (int)n, p);
		if (n && bios_there(path, &st)) {
			const unsigned *want = k < CFG_BIOS_NAMES ? s->bios_bytes[k] : NULL;

			if (r->state == BIOS_WRONG_SIZE) {
				/* After the bad file: the first name past it that is
				 * there, which the core takes once it is deleted */
				if (want && want[0] && size_good(want, st.st_size))
					snprintf(r->good, sizeof r->good, "%.*s", (int)n, p);
				return r->state;
			}
			if (!want || !want[0] || size_good(want, st.st_size)) return r->state = BIOS_OK;
			snprintf(r->name, sizeof r->name, "%.*s", (int)n, p);
			r->have = (unsigned long long)st.st_size;
			r->want = want[0];
			r->state = BIOS_WRONG_SIZE;
		}
		p += n;
		if (*p) p++;
	}
	if (r->state == BIOS_WRONG_SIZE) return r->state;
	snprintf(r->name, sizeof r->name, "%.*s", (int)strcspn(s->disc_bios, ","), s->disc_bios);
	return r->state = BIOS_MISSING;
}

static void size_bytes(unsigned long long bytes, char *out, size_t n)
{
	snprintf(out, n, TN(S_LAUNCH_BIOS_BYTES, (long)bytes), bytes);
}

void bios_size_text(unsigned long long bytes, char *out, size_t n)
{
	if (bytes < 1024)
		size_bytes(bytes, out, n);
	else if (bytes % (1024ull * 1024) == 0)
		snprintf(out, n, T(S_SYSMENU_SIZE_MB), bytes / (1024ull * 1024));
	else
		snprintf(out, n, T(S_SYSMENU_SIZE_KB), bytes / 1024);
}

void bios_message(const bios_result *r, char *out, size_t n)
{
	char have[48], want[48];

	out[0] = '\0';
	if (r->state == BIOS_MISSING) {
		snprintf(out, n, T(S_LAUNCH_NEEDS_BIOS), (int)strlen(r->name), r->name);
		return;
	}
	if (r->state != BIOS_WRONG_SIZE) return;
	bios_size_text(r->have, have, sizeof have);
	bios_size_text(r->want, want, sizeof want);
	/* 524289 bytes is "512 KB" too: then both in bytes, which differ */
	if (!strcmp(have, want)) {
		size_bytes(r->have, have, sizeof have);
		size_bytes(r->want, want, sizeof want);
	}
	if (r->good[0])
		snprintf(out, n, T(S_LAUNCH_BIOS_HIDES), r->name, have, r->good);
	else
		snprintf(out, n, T(S_LAUNCH_BIOS_SIZE), r->name, have, want);
}

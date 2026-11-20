/* SPDX-License-Identifier: GPL-3.0-only */
#include "about.h"
#include "i18n.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

bool about_storage_counts(about_info *i, uint64_t unit, uint64_t blocks,
                          uint64_t free_blocks, uint64_t available_blocks)
{
	i->storage_known = false;
	i->total_bytes = i->used_bytes = i->free_bytes = 0;
	if (!unit || !blocks || blocks > UINT64_MAX / unit ||
	    free_blocks > blocks || available_blocks > free_blocks) return false;
	i->total_bytes = blocks * unit;
	i->used_bytes = (blocks - free_blocks) * unit;
	i->free_bytes = available_blocks * unit;
	i->storage_known = true;
	return true;
}

/* One whole number from a sysfs file. Missing, negative, overflowing and
 * partial numbers are all false, rather than a claim about battery wear. */
static bool number_read(const char *dir, const char *name, uint64_t *value)
{
	char path[1024], text[64], *start, *end;
	size_t len;
	FILE *f;
	int n;

	if (!dir) return false;
	n = snprintf(path, sizeof path, "%s/%s", dir, name);
	if (n < 0 || (size_t)n >= sizeof path || !(f = fopen(path, "r"))) return false;
	len = fread(text, 1, sizeof text - 1, f);
	if (!len || memchr(text, '\0', len)) { fclose(f); return false; }
	text[len] = '\0';
	/* Do not accept a truncated value or a second non-whitespace line. */
	while ((n = fgetc(f)) != EOF)
		if (!isspace((unsigned char)n)) { fclose(f); return false; }
	if (ferror(f)) { fclose(f); return false; }
	fclose(f);
	start = text;
	while (isspace((unsigned char)*start)) start++;
	if (!isdigit((unsigned char)*start)) return false;
	errno = 0;
	*value = strtoull(start, &end, 10);
	if (errno == ERANGE) return false;
	while (isspace((unsigned char)*end)) end++;
	return !*end;
}

/* sysfs uses microamp-hours; zero is unavailable too. */
static uint64_t charge_read(const char *dir, const char *name)
{
	uint64_t value;

	return number_read(dir, name, &value) ? value : 0;
}

void about_read(about_info *i, const char *card, const char *battery_dir)
{
	struct statvfs st;

	memset(i, 0, sizeof *i);
	if (card && statvfs(card, &st) == 0)
		about_storage_counts(i, st.f_frsize ? st.f_frsize : st.f_bsize,
		                     st.f_blocks, st.f_bfree, st.f_bavail);
	i->full_uah = charge_read(battery_dir, "charge_full");
	i->design_uah = charge_read(battery_dir, "charge_full_design");
	{
		uint64_t n;

		i->learned = number_read(battery_dir, "charge_full_learned", &n) ?
		             (n > 1000 ? 1000 : (int)n) : -1;
	}
}

static void storage_text(char *out, size_t cap, uint64_t bytes)
{
	/* Decimal GB, rounded to a tenth without multiplying the byte count. */
	uint64_t tenths = bytes / 100000000u + (bytes % 100000000u >= 50000000u);
	snprintf(out, cap, "%" PRIu64 ".%" PRIu64 " GB", tenths / 10, tenths % 10);
}

static void capacity_text(char *out, size_t cap, uint64_t uah)
{
	char raw[32], grouped[40];
	uint64_t mah = uah / 1000 + (uah % 1000 >= 500);
	size_t n, j = 0;

	snprintf(raw, sizeof raw, "%" PRIu64, mah);
	n = strlen(raw);
	for (size_t k = 0; k < n; k++) {
		if (k && (n - k) % 3 == 0) grouped[j++] = ',';
		grouped[j++] = raw[k];
	}
	grouped[j] = '\0';
	snprintf(out, cap, "%s mAh", grouped);
}

int about_detail_rows(const about_info *i, menu_row rows[ABOUT_DETAIL_ROWS], about_values *b)
{
	static const str_id labels[ABOUT_DETAIL_ROWS] = {
		S_ABOUT_STORAGE_USED, S_ABOUT_STORAGE_FREE, S_ABOUT_STORAGE_TOTAL,
		S_ABOUT_BATTERY_HEALTH, S_ABOUT_FULL_CHARGE, S_ABOUT_DESIGN_CHARGE
	};
	for (int k = 0; k < ABOUT_DETAIL_ROWS; k++) {
		snprintf(b->value[k], sizeof b->value[k], "%s", T(S_ABOUT_UNKNOWN));
		rows[k] = (menu_row){ T(labels[k]), b->value[k], false };
	}
	if (i->storage_known) {
		storage_text(b->value[0], sizeof b->value[0], i->used_bytes);
		storage_text(b->value[1], sizeof b->value[1], i->free_bytes);
		storage_text(b->value[2], sizeof b->value[2], i->total_bytes);
	}
	if (i->full_uah) capacity_text(b->value[4], sizeof b->value[4], i->full_uah);
	if (i->design_uah) capacity_text(b->value[5], sizeof b->value[5], i->design_uah);
	/* Never measured: the full charge is the design's guess, so neither it
	 * nor a health from it says anything about this battery */
	if (i->learned == 0 && i->full_uah && i->design_uah && i->full_uah >= i->design_uah) {
		snprintf(b->value[3], sizeof b->value[3], "%s", T(S_ABOUT_NOT_MEASURED));
		snprintf(b->value[4], sizeof b->value[4], "%s", T(S_ABOUT_NOT_MEASURED));
	} else if (i->full_uah && i->design_uah) {
		long double pct = (long double)i->full_uah / (long double)i->design_uah * 100.0L;
		unsigned health = pct >= 100.0L ? 100u : (unsigned)(pct + 0.5L);
		snprintf(b->value[3], sizeof b->value[3], T(S_ABOUT_HEALTH_ESTIMATE), health);
	}
	return ABOUT_DETAIL_ROWS;
}

/* SPDX-License-Identifier: GPL-3.0-only */
/* When Slow, the Nintendo DS shelf's row (src/dsslow.h): its two choices,
 * the words the database keeps and RUN's frameskip= says for them, a word
 * that is none being Keep Speed, and stepping round both ways. */
#include <stdio.h>
#include <string.h>

#include "../src/dsslow.h"

static int fails, checks;

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

int main(void)
{
	printf("When Slow:\n");
	ck(DSS_CHOICES == 2, "two choices");
	ck(!strcmp(dsslow_id(DSS_KEEP), "keep") && !strcmp(dsslow_id(DSS_STEADY), "steady"),
	   "kept as keep and steady");
	ck(dsslow_parse("keep") == DSS_KEEP && dsslow_parse("steady") == DSS_STEADY, "and read back");
	ck(dsslow_parse(NULL) == DSS_KEEP && dsslow_parse("") == DSS_KEEP && dsslow_parse("late") == DSS_KEEP,
	   "a word that is none is Keep Speed, the default");
	ck(!strcmp(dsslow_frameskip(DSS_KEEP), "late"), "Keep Speed sends frameskip=late, as always");
	ck(!strcmp(dsslow_frameskip(DSS_STEADY), "steady"), "Steady 30 sends frameskip=steady");
	ck(dsslow_step(DSS_KEEP, 1) == DSS_STEADY && dsslow_step(DSS_STEADY, 1) == DSS_KEEP,
	   "Right goes round");
	ck(dsslow_step(DSS_KEEP, -1) == DSS_STEADY && dsslow_step(DSS_STEADY, -1) == DSS_KEEP,
	   "and so does Left");
	ck(dsslow_step((dss_choice)7, 1) == DSS_STEADY, "a choice that is none steps from Keep Speed");
	ck(dsslow_label_id(DSS_KEEP) == S_SYSMENU_DSSLOW_KEEP &&
	   dsslow_label_id(DSS_STEADY) == S_SYSMENU_DSSLOW_STEADY, "each its own value on the row");
	if (fails) {
		printf("dsslow: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("dsslow: %d checks passed\n", checks);
	return 0;
}

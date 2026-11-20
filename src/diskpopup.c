/* SPDX-License-Identifier: GPL-3.0-only */
/* A Famicom Disk System disk's pop-up. See src/diskpopup.h. */
#include "diskpopup.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"

bool disk_popup_words(const char *state, int disk, char side, const char **head,
                      char *body, size_t n)
{
	if (!state || !head || !body || !n) return false;
	body[0] = '\0';
	if (!strcmp(state, "eject-first")) {
		*head = T(S_POPUP_DISK_SIDE);
		snprintf(body, n, "%s", T(S_POPUP_DISK_EJECT_FIRST));
		return true;
	}
	if (disk < 1 || disk > 4 || (side != 'A' && side != 'B')) return false;
	if (!strcmp(state, "selected"))  *head = T(S_POPUP_DISK_SIDE);
	else if (!strcmp(state, "in"))   *head = T(S_POPUP_DISK_IN);
	else if (!strcmp(state, "out"))  *head = T(S_POPUP_DISK_OUT);
	else return false;
	snprintf(body, n, T(S_POPUP_DISK_WHICH), disk, side);
	return true;
}

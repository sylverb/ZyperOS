/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_RMTREE_H
#define ZYPEROS_RMTREE_H

#include <stdbool.h>

/* `path` and everything under it removed, what is in a folder before the
 * folder, walked by the folders' descriptors rather than by paths. A link
 * is removed and never followed, `path` itself too: one that leads to the
 * card's key folder leaves that folder and its files as they are. Nothing
 * on another file system under it is touched (a bind mount stays, and with
 * it the folder that holds it), and a folder `path` names that this
 * process does not own is left alone. True when `path` is gone, or never
 * was there; `path` has a folder before its last name ("/tmp/x"). */
bool rm_tree(const char *path);

#endif

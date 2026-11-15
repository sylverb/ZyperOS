/* SPDX-License-Identifier: GPL-3.0-only */
/* See rmtree.h. Walked by the folders' own descriptors, never by a path
 * built from names: each entry is looked at, opened and removed relative to
 * the folder it is in (fstatat, openat, unlinkat), a folder is opened with
 * O_NOFOLLOW and checked to be the one just looked at, so a folder swapped
 * for a link part way can only make the walk stop, never lead it out. It
 * runs as root under /tmp. */
#if !defined(_GNU_SOURCE) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "rmtree.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEPTH_MAX 32    /* far past anything a session leaves in ~ */

/* `name` in the folder `at`, whole: a file or a link unlinked, a folder on
 * the file system `dev` emptied and removed. False when something stayed. */
static bool rm_at(int at, const char *name, dev_t dev, int depth)
{
	struct stat st, opened;
	struct dirent *e;
	DIR *d;
	bool ok = true;
	int fd;

	if (fstatat(at, name, &st, AT_SYMLINK_NOFOLLOW) != 0) return errno == ENOENT;
	if (!S_ISDIR(st.st_mode)) return unlinkat(at, name, 0) == 0 || errno == ENOENT;
	/* A mount under it stays, and so does the folder that holds it */
	if (st.st_dev != dev || depth > DEPTH_MAX) return false;
	fd = openat(at, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (fd < 0) return false;
	if (fstat(fd, &opened) != 0 || opened.st_dev != st.st_dev || opened.st_ino != st.st_ino) {
		close(fd);
		return false;
	}
	if (!(d = fdopendir(fd))) {
		close(fd);
		return false;
	}
	while ((e = readdir(d))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
		ok = rm_at(dirfd(d), e->d_name, dev, depth + 1) && ok;
	}
	closedir(d);
	return unlinkat(at, name, AT_REMOVEDIR) == 0 && ok;
}

bool rm_tree(const char *path)
{
	char parent[PATH_MAX];
	const char *slash = strrchr(path, '/'), *base;
	struct stat st;
	bool ok;
	int at;

	if (!slash || !slash[1]) return false;          /* a name to remove, under a folder */
	base = slash + 1;
	if (slash == path) snprintf(parent, sizeof parent, "/");
	else if (snprintf(parent, sizeof parent, "%.*s", (int)(slash - path), path) >= (int)sizeof parent)
		return false;
	at = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (at < 0) return errno == ENOENT;
	if (fstatat(at, base, &st, AT_SYMLINK_NOFOLLOW) != 0) {
		ok = errno == ENOENT;
		close(at);
		return ok;
	}
	/* Only a folder of this process's own: one somebody else made in
	 * /tmp under the same name is theirs, and stays */
	if (S_ISDIR(st.st_mode) && st.st_uid != geteuid()) {
		close(at);
		return false;
	}
	ok = rm_at(at, base, st.st_dev, 0);
	close(at);
	return ok;
}

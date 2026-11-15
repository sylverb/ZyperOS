/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_SPAWN_H
#define ZYPEROS_SPAWN_H

#include <stdbool.h>
#include <stddef.h>

/* Run argv[0] with argv, by fork and execv and never a shell: what reaches
 * here is an SSID or a device name from the air, and this process is root
 * (src/wifi.c says why). Its stdout and stderr go into `out`, which is always
 * NUL-terminated (none kept when out is NULL); the rest is read and dropped.
 *
 * Returns its exit status, -1 if it could not be run, and -2 if it was still
 * going `timeout_s` seconds on and was killed. 0 or less is no deadline, for
 * a tool that always exits on its own. */
int spawn_capture(char *const argv[], char *out, size_t cap, int timeout_s);

/* Start argv[0] with argv beside this process and leave it running: a
 * server the launcher owns for a while (NES Player 2's p2d). Never a shell,
 * as above. Its stdin is /dev/null and its stdout and stderr are this
 * process's, so its lines are in the launcher's log; nothing else of this
 * process's is open in it (the display, the sockets), every signal is as a
 * fresh process has it, and on Linux it is sent SIGTERM if this process dies
 * first. Returns its pid, or -1 if it could not be started. */
long spawn_child(char *const argv[]);

/* Start argv[0] with argv, and with envp when one is given (this process's
 * environment otherwise), as a process of its own that outlives this one and
 * leaves no zombie: forked twice, the middle child reaped here and the
 * program inherited by init, in a session of its own, in workdir when one is
 * given. Never a shell. Its stdin, stdout and stderr are this process's;
 * nothing else of this process's is open in it.
 *
 * Returns the program's pid once its exec is done, or -1 when it could not
 * be started (errno the exec's, when that is what failed), so a caller can
 * write the pid down as the program's: the resident Diatom the launcher
 * starts again after a crash (src/launch_screen.c), whose pid launch.sh and
 * src/resident.c find in /tmp/diatom.pid. */
long spawn_detached(char *const argv[], char *const envp[], const char *workdir);

/* Whether the child `pid` has ended, reaped if so, with its exit status in
 * `*status` (-1 for one a signal ended). Never waits. */
bool spawn_ended(long pid, int *status);

#endif

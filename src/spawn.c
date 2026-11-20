/* SPDX-License-Identifier: GPL-3.0-only */
/* See spawn.h. The Wi-Fi's wpa_cli and Bluetooth's bluetoothctl, NES
 * Player 2's p2d, and the launcher's detached children (MP4, a resident
 * Diatom started again). */
#include "spawn.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

/* The deadline is not defensive programming, it is a bug fix. On a fresh card
 * Bluetooth is off, so launch.sh never runs hciattach and there is no adapter;
 * `bluetoothctl power on` in that state does not fail and does not exit, it
 * WAITS for a controller that is never coming. The launcher sat in waitpid
 * behind it and the screen froze on "Turning on...", needing the process
 * killed over adb. Seen on the device 2026-09-06.
 *
 * bluetoothctl has a --timeout of its own and it is used for scanning, but
 * relying on the tool to bound itself is what produced the hang. This bounds
 * it from outside, where a tool that ignores its own timeout cannot reach. */
int spawn_capture(char *const argv[], char *out, size_t cap, int timeout_s)
{
	int fd[2], status = -1;
	pid_t pid;
	size_t used = 0;
	struct timespec t0, now;
	bool killed = false;

	if (out && cap) out[0] = '\0';
	if (pipe(fd) < 0) return -1;

	pid = fork();
	if (pid < 0) { close(fd[0]); close(fd[1]); return -1; }
	if (pid == 0) {
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		dup2(fd[1], STDERR_FILENO);
		close(fd[1]);
		execv(argv[0], argv);
		_exit(127);
	}
	close(fd[1]);
	clock_gettime(CLOCK_MONOTONIC, &t0);

	for (;;) {
		ssize_t n;
		char scratch[256];
		char *dst;
		size_t room;

		if (timeout_s > 0) {
			struct pollfd pfd = { fd[0], POLLIN, 0 };

			clock_gettime(CLOCK_MONOTONIC, &now);
			if (now.tv_sec - t0.tv_sec >= timeout_s) {
				/* SIGKILL rather than SIGTERM: a bluetoothctl waiting on a
				 * controller has already shown it is not minded to leave. */
				kill(pid, SIGKILL);
				killed = true;
				break;
			}
			if (poll(&pfd, 1, 200) <= 0) continue;
		}
		dst  = (out && used + 1 < cap) ? out + used : scratch;
		room = (out && used + 1 < cap) ? cap - used - 1 : sizeof scratch;
		n = read(fd[0], dst, room);
		if (n <= 0) break;
		if (dst != scratch) used += (size_t)n;
	}
	if (out && cap) out[used] = '\0';
	close(fd[0]);
	if (waitpid(pid, &status, 0) < 0) return -1;
	if (killed) return -2;
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Between fork and exec only system calls: the launcher has threads, and
 * nothing else is safe in the child of one (sshlink.c's dropbear the same). */
long spawn_child(char *const argv[])
{
	struct sigaction dfl;
	struct rlimit lim;
	sigset_t none;
	pid_t pid;
	int null, fd, maxfd, sig;

	sigemptyset(&none);
	memset(&dfl, 0, sizeof dfl);
	dfl.sa_handler = SIG_DFL;
	maxfd = getrlimit(RLIMIT_NOFILE, &lim) == 0 && lim.rlim_cur != RLIM_INFINITY &&
	        lim.rlim_cur < 65536 ? (int)lim.rlim_cur : 65536;
	pid = fork();
	if (pid < 0) return -1;
	if (pid > 0) return (long)pid;

#ifdef __linux__
	/* It ends with the launcher, whatever ends that */
	prctl(PR_SET_PDEATHSIG, SIGTERM);
	if (getppid() == 1) _exit(1);
#endif
	null = open("/dev/null", O_RDONLY);
	if (null >= 0) { dup2(null, 0); if (null > 2) close(null); }
	/* Nothing of the launcher's past stderr: its display, its sockets, a
	 * port it listens on */
	for (fd = 3; fd < maxfd; fd++) close(fd);
	/* An ignored signal stays ignored across exec, and a blocked one blocked */
	sigprocmask(SIG_SETMASK, &none, NULL);
	for (sig = 1; sig < NSIG; sig++)
		if (sig != SIGKILL && sig != SIGSTOP) sigaction(sig, &dfl, NULL);
	execv(argv[0], argv);
	_exit(127);
}

/* All of n bytes, or fewer at end of file: what was read, -1 on an error */
static ssize_t read_all(int fd, void *buf, size_t n)
{
	size_t got = 0;

	while (got < n) {
		ssize_t r = read(fd, (char *)buf + got, n - got);

		if (r < 0 && errno == EINTR) continue;
		if (r < 0) return -1;
		if (r == 0) break;
		got += (size_t)r;
	}
	return (ssize_t)got;
}

/* EVERYTHING ABOVE STDERR IS CLOSED IN THE PROGRAM, before it becomes the
 * program asked for.
 *
 * Fork hands a child every descriptor this process has open: the resident
 * emulator's socket, MP4's, the display, the GPU, four input devices, the
 * font. A short-lived child drops them when it exits. A DAEMON keeps them for
 * as long as it lives, and MP4 is meant to outlive the launcher. That froze
 * the Brick on 2026-09-18: MP4 held a dead launcher's end of Diatom's socket,
 * so Diatom never saw it close, and the restarted launcher blocked in connect
 * before its first frame. Diatom's half is that the newest connection wins;
 * this is ours.
 *
 * By number up to the limit, not by listing /proc/self/fd: listing allocates,
 * and after fork in a threaded process only async-signal-safe calls are safe -
 * close is one. The limit is read here, before fork, for the same reason. 1024
 * on the Brick, with about thirty open. stdout and stderr stay: they are the
 * log, and a daemon's lines belong in it.
 *
 * The pid comes back over a pipe, from the program itself: the parent knows
 * only the middle child's. The program writes its own pid first, then execs;
 * the write end is close-on-exec, so a done exec is the end of the pipe, and a
 * failed one sends its errno before the end. One writer, so the two cannot
 * arrive out of order. A pid written down for a program that never ran is
 * what made launch.sh start a second resident Diatom (2026-11-14), and a pid
 * file is only worth what it names. */
long spawn_detached(char *const argv[], char *const envp[], const char *workdir)
{
	struct rlimit lim;
	int fd[2], top, err = 0;
	pid_t mid, pid = -1;

	top = getrlimit(RLIMIT_NOFILE, &lim) == 0 && lim.rlim_cur != RLIM_INFINITY &&
	      lim.rlim_cur < 65536 ? (int)lim.rlim_cur : 65536;
	if (pipe(fd) < 0) return -1;
	fcntl(fd[0], F_SETFD, FD_CLOEXEC);
	fcntl(fd[1], F_SETFD, FD_CLOEXEC);
	mid = fork();
	if (mid < 0) {
		close(fd[0]);
		close(fd[1]);
		return -1;
	}
	if (mid == 0) {
		close(fd[0]);
		if (fork() == 0) {
			int i;

			pid = getpid();
			if (write(fd[1], &pid, sizeof pid) < 0) { /* the parent reads none */ }
			setsid();
			for (i = 3; i < top; i++)
				if (i != fd[1]) close(i);
			if (workdir && chdir(workdir) != 0) { /* still try */ }
			if (envp) execve(argv[0], argv, envp);
			else execv(argv[0], argv);
			err = errno;
			if (write(fd[1], &err, sizeof err) < 0) { /* reported as started */ }
			_exit(127);
		}
		_exit(0);
	}
	close(fd[1]);
	if (read_all(fd[0], &pid, sizeof pid) != (ssize_t)sizeof pid) pid = -1;
	/* Then the end of the pipe (the exec done), or the exec's errno */
	if (pid > 0 && read_all(fd[0], &err, sizeof err) == (ssize_t)sizeof err) pid = -1;
	close(fd[0]);
	while (waitpid(mid, NULL, 0) < 0 && errno == EINTR) { }
	if (pid <= 0) {
		errno = err ? err : ECHILD;
		return -1;
	}
	return (long)pid;
}

bool spawn_ended(long pid, int *status)
{
	int st = 0;
	pid_t r;

	if (pid <= 0) return true;
	r = waitpid((pid_t)pid, &st, WNOHANG);
	if (r == 0) return false;
	if (status) *status = r < 0 ? -1 : WIFEXITED(st) ? WEXITSTATUS(st) : -1;
	return true;
}

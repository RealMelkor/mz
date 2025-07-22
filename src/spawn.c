/*
 * Copyright (c) 2025 RMF <rawmonk@rmf-dev.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <spawn.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include "spawn.h"

extern char **environ;

int spawn(char *program, int wait, int closefds, ...) {

	int err, n, i;
	pid_t pid;
	char **argv;
	posix_spawn_file_actions_t action;
	va_list args;

	posix_spawn_file_actions_init(&action);
	if (closefds) {
		posix_spawn_file_actions_addclose(&action, STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&action, STDIN_FILENO);
		posix_spawn_file_actions_addclose(&action, STDERR_FILENO);
	}

	va_start(args, closefds);
	for (n = 0; va_arg(args, char*); n++) ;
	va_end(args);

	argv = malloc(sizeof(char*) * (n + 2));
	if (!argv) return -1;
	argv[0] = program;

	va_start(args, closefds);
	for (i = 0; i < n; i++)
		argv[1 + i] = va_arg(args, char*);
	va_end(args);

	argv[1 + i] = NULL;

	err = posix_spawnp(&pid, argv[0], &action, NULL, argv, environ);

	free(argv);

	if (!err && wait) waitpid(pid, NULL, 0);

	return err;
}

int spawn_pipe(char *program, const char *data, int wait, int closefds, ...) {

	int err, n, i;
	pid_t pid;
	char **argv;
	posix_spawn_file_actions_t action;
	va_list args;
	int fds[2];

	if (pipe(fds)) return -1;

	posix_spawn_file_actions_init(&action);
	if (closefds) {
		posix_spawn_file_actions_addclose(&action, STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&action, STDERR_FILENO);
	}
	posix_spawn_file_actions_addclose(&action, STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&action, fds[0], STDIN_FILENO);
	posix_spawn_file_actions_addclose(&action, fds[0]);
	posix_spawn_file_actions_addclose(&action, fds[1]);

	va_start(args, closefds);
	for (n = 0; va_arg(args, char*); n++) ;
	va_end(args);

	argv = malloc(sizeof(char*) * (n + 2));
	if (!argv) return -1;
	argv[0] = program;

	va_start(args, closefds);
	for (i = 0; i < n; i++)
		argv[1 + i] = va_arg(args, char*);
	va_end(args);

	argv[1 + i] = NULL;

	err = posix_spawnp(&pid, argv[0], &action, NULL, argv, environ);

	free(argv);

	if (!err) {
		n = strnlen(data, 1024);
		if (write(fds[1], data, n) != n) return -1;
	}

	close(fds[1]);
	close(fds[0]);

	if (!err && wait) waitpid(pid, NULL, 0);

	return err;
}

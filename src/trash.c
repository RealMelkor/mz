/*
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
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
#ifdef __linux__
#define _GNU_SOURCE
#else
#define _BSD_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "trash.h"
#include "client.h"
#include "strlcpy.h"
#include "util.h"

int trash_init() {
	char *env;
	int home, trash;

	env = getenv("HOME");
	if (!env) {
		printf("home folder not found\n");
		return -1;
	}

	home = open(env, O_DIRECTORY);
	if (home < 0) goto fail;

	trash = openat(home, ".trash", O_DIRECTORY);
	if (trash > -1)  {
		close(home);
		return trash;
	}

	if (mkdirat(home, ".trash", 0700)) goto fail;

	trash = openat(home, ".trash", O_DIRECTORY);
	if (trash < 0) goto fail;

	return trash;
fail:
	if (home > -1)
		close(home);
	printf("%s\n", strerror(errno));
	return -1;
}

int trash_send(int fd, char *path, char *name) {

	char buf[1024], id[32];
	int info, len;

	info = openat(client.trash, "info", O_WRONLY|O_CREAT|O_APPEND, 0700);
	if (info < 0)
		return -1;

	do {
		size_t i;
		int try;

		i = 0;
		while (i < sizeof(id) - 1) {
			id[i] = 'a' + rand() % 26;
			i++;
		}
		id[sizeof(id) - 1] = '\0';

		/* check if there's not already a file with that id */
		try = openat(client.trash, id, O_RDONLY);
		if (try < 0) break;
		close(i);
	} while (1);

	if (renameat(fd, name, client.trash, id))
		return -1;

	len = snprintf(V(buf), "%s %s/%s\n", id, path, name);
	len = write(info, buf, len) != len;
	close(info);

	return -len;
}

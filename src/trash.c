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
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fts.h>
#include "trash.h"
#include "client.h"
#include "strlcpy.h"
#include "util.h"
#include <stdint.h>
#include "termbox.h"

int recursive_delete(const char *dir) {

	int ret;
	FTS *ftsp;
	FTSENT *curr;
	char *files[2];

	files[0] = (char*)dir;
	files[1] = NULL;

	ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
	if (!ftsp) {
		ret = -1;
		goto clear;
	}

	ret = 0;
	while ((curr = fts_read(ftsp))) {
		switch (curr->fts_info) {
		case FTS_NS:
		case FTS_DNR:
		case FTS_ERR:
			break;
		case FTS_DC:
		case FTS_DOT:
		case FTS_NSOK:
			break;
		case FTS_D:
			break;
		case FTS_DP:
		case FTS_F:
		case FTS_SL:
		case FTS_SLNONE:
		case FTS_DEFAULT:
			if (remove(curr->fts_accpath) < 0)
				ret = -1;
			break;
		}
	}

clear:
	if (ftsp)
		fts_close(ftsp);

	return ret;
}

static int gethome(char *buf, size_t length) {

        struct passwd *pw;
	char *home;
	int fd;

	home = getenv("HOME");
	if (home) {
		fd = open(home, O_DIRECTORY);
		if (fd > -1) {
			close(fd);
			return strlcpy(buf, home, length);
		}
	}

	pw = getpwuid(geteuid());
        if (!pw) return -1;
        fd = open(pw->pw_dir, O_DIRECTORY);
	if (fd < 0) {
		close(fd);
		return -1;
	}
        return strlcpy(buf, pw->pw_dir, length);
}

static int trash_path(char *path, size_t length) {
	int len = gethome(path, length);
	if (len == -1) return -1;

	strlcpy(&path[length], "/.trash", sizeof(path) - length);
	return 0;
}

int trash_init() {
	char path[PATH_MAX];
	int home, trash;

	if (gethome(V(path)) == -1) return -1;

	home = open(path, O_DIRECTORY);
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

int trash_clear() {

	char path[PATH_MAX];

	if (trash_path(V(path))) return -1;
	if (recursive_delete(path)) return -1;

	close(client.trash);
	client.trash = trash_init();
	if (client.trash < 0) return -1;

	return 0;
}

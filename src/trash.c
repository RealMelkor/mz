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
#include "client.h"
#include "strlcpy.h"
#include "file.h"
#include "view.h"
#include "trash.h"
#include "util.h"

#define TRASH "/.trash"
#define ID_LENGTH 32

int recursive_delete(const char *dir) {

	int ret;
	FTS *ftsp;
	FTSENT *curr;
	char *files[2];

	files[0] = (char*)dir;
	files[1] = NULL;

	ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
	if (!ftsp)
		return -1;

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

	char buf[1024];

	int len = gethome(buf, length);
	if (len == -1 || len + sizeof(TRASH) >= length) goto clean;

	len = strlcpy(path, buf, length);
	strlcpy(&path[len], TRASH, sizeof(path) - length);

	if (!strncmp(path, buf, length)) goto clean;

	return 0;
clean: /* make sure we're not deleting a whole folder by accident */
	memset(path, 0, length);
	strlcpy(path, "/nonexistent/folder", sizeof(path));
	return -1;
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

	char buf[PATH_MAX * 2], id[ID_LENGTH + 1];
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

int trash_restore(struct view *view) {

	size_t i;
	char path[PATH_MAX];
	int error = 0;

	if (view->fd != TRASH_FD) {
		errno = EINVAL;
		return -1;
	}

	if (trash_path(V(path))) return -1;

	i = 0;
	while (i < view->length) {

		char src[PATH_MAX];
		char id[ID_LENGTH + 1];
		size_t j = i++;
		int fd;

		if (!view->entries[j].selected) continue;
		memcpy(id, &((char*)view->other)[j * ID_LENGTH], ID_LENGTH);
		id[ID_LENGTH] = '\0';
		snprintf(V(src), "%s/%s", path, id);

		/* check if file exist before using rename */
		fd = open(view->entries[j].name, 0);
		if (fd > -1) {
			close(fd);
			errno = EEXIST;
			error = -1;
			continue;
		}

		if (rename(src, view->entries[j].name)) return -1;
		view->entries[j].selected = -1;
	}
	return error;
}

int trash_refresh(struct view *view) {

	void *next, *prev;
	size_t i;
	int fd, rewrite;

	i = 0;
	rewrite = 0;
	while (i < view->length) {
		switch (view->entries[i].selected) {
		case -1:
			rewrite = 1;
			break;
		case 1:
			view->entries[i].selected = 0;
			break;
		}
		i++;
	}
	if (!rewrite) return 0;

	/* rewrite info file */
	fd = openat(client.trash, "info", O_CREAT|O_WRONLY|O_TRUNC);
	if (!fd) return -1;

	i = 0;
	while (i < view->length) {

		char c;
		size_t j = i++;

		if (view->entries[j].selected == -1) continue;

		write(fd, &((char*)view->other)[j * ID_LENGTH], ID_LENGTH);
		c = ' ';
		write(fd, &c, 1);
		write(fd, view->entries[j].name,
			strnlen(V(view->entries[j].name)));
		c = '\n';
		write(fd, &c, 1);
	}
	close(fd);

	free(view->other);
	free(view->entries);

	next = view->next;
	prev = view->prev;
	trash_view(view);
	view->next = next;
	view->prev = prev;

	return 0;
}

int trash_view(struct view* view) {

	int fd;
	size_t i;
	char buf[PATH_MAX * 2];

	PZERO(view);
	sstrcpy(view->path, "Trash");
	view->fd = TRASH_FD;

	fd = openat(client.trash, "info", O_RDONLY);
	if (fd < 0) return 0;

	i = 0;
	while (i < sizeof(buf)) {

		void *ptr;
		char id[ID_LENGTH];
		char c;
		size_t j;

		j = read(fd, V(id));
		if (!j) { /* success : end of file */
			close(fd);
			return 0;
		}
		if (j != sizeof(id)) break;

		j = 0;
		while (j < sizeof(id)) {
			if (id[j] > 'z' || id[j] < 'a') break;
			j++;
		}
		if (j != sizeof(id)) break;

		if (read(fd, &c, 1) != 1 || c != ' ') break;

		j = 0;
		while (j < sizeof(buf)) {
			if (read(fd, &c, 1) != 1) break;
			if (c == '\n') {
				buf[j] = 0;
				break;
			}
			buf[j] = c;
			j++;
		}

		ptr = realloc(view->entries, sizeof(struct entry) * (i + 1));
		if (!ptr) break;
		view->entries = ptr;
		RZERO(view->entries[i]);

		sstrcpy(view->entries[i].name, buf);
		view->entries[i].type = 0;
		view->length = i + 1;

		ptr = realloc(view->other, (i + 1) * ID_LENGTH + 1);
		if (!ptr) break;
		view->other = ptr;
		memcpy(&((char*)view->other)[i * ID_LENGTH], V(id));

		i++;
	}

	free(view->other);
	free(view->entries);
	close(fd);
	return -1;
}

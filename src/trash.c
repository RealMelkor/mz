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
#include <dirent.h>
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
	if (fd < 0) return -1;
	close(fd);
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

	close(home);
	return trash;
fail:
	if (home > -1)
		close(home);
	return -1;
}

int trash_send(int fd, char *path, char *name) {

	char buf[PATH_MAX * 2], id[ID_LENGTH + 1];
	int info, len, error;

	info = openat(client.trash, "info", O_WRONLY|O_CREAT|O_APPEND, 0600);
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

	trash_path(V(buf));
	error = file_move(path, fd, name, client.trash, buf, id);
	if (!error) {
		len = snprintf(V(buf), "%s %s/%s\n", id, path, name);
		error = -(write(info, buf, len) != len);
	}
	close(info);

	return error;
}

int trash_clear() {

	char path[PATH_MAX], cmd[PATH_MAX * 2];

	if (trash_path(V(path))) return -1;
	snprintf(V(cmd), "rm -r %s", path);
	if (system(cmd)) return -1;

	close(client.trash);
	client.trash = trash_init();
	if (client.trash < 0) return -1;

	return 0;
}

int trash_rawpath(struct view *view, char *out, size_t length) {

	char path[PATH_MAX];

	if (view->fd != TRASH_FD) return -1;
	if (trash_path(V(path))) return -1;
	if (snprintf(out, length, "%s/%s", path,
			(char*)SELECTED(view).other) >= (int)length)
		return -1;
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
		char *id;
		size_t j = i++;
		int fd;

		if (!view->entries[j].selected) continue;
		id = view->entries[j].other;
		snprintf(V(src), "%s/%s", path, id);

		/* check if file exist before using rename */
		fd = open(view->entries[j].name, 0);
		if (fd > -1) {
			close(fd);
			errno = EEXIST;
			error = -1;
			continue;
		}

		if (rename(src, view->entries[j].name)) {
			char *name;
			int ret;
			if (errno != EXDEV) return -1;
			STRCPY(src, view->entries[j].name);
			name = strrchr(src, '/');
			if (!name) return -1;
			*name = '\0';
			name++;
			fd = open(src, O_DIRECTORY);
			if (fd < 0) return -1;
			ret = file_move(path, client.trash, id, fd, src, name);
			close(fd);
			if (ret < 0) return -1;
		}
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

		write(fd, view->entries[j].other, ID_LENGTH);
		c = ' ';
		write(fd, &c, 1);
		write(fd, view->entries[j].name,
			strnlen(V(view->entries[j].name)));
		c = '\n';
		write(fd, &c, 1);
	}
	close(fd);

	i = 0;
	while (i < view->length) {
		free(view->entries[i].other);
	}

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
	STRCPY(view->path, "Trash");
	view->fd = TRASH_FD;

	fd = openat(client.trash, "info", O_RDONLY);
	if (fd < 0) return 0;

	i = 0;
	while (1) {

		void *ptr;
		char id[ID_LENGTH];
		char c;
		size_t j;

		j = read(fd, V(id));
		if (!j) { /* success : end of file */
			close(fd);
			qsort(view->entries, view->length,
				sizeof(struct entry), file_sort);
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

		STRCPY(view->entries[i].name, buf);
		view->entries[i].type = DT_REG;
		{
			struct stat s;
			char path[ID_LENGTH + 1];
			memcpy(path, id, ID_LENGTH);
			path[ID_LENGTH] = 0;
			view->entries[i].type =
				fstatat(client.trash, path, &s, 0) ?
					DT_REG : (S_ISDIR(s.st_mode) ?
						DT_DIR : DT_REG);
		}
		view->length = i + 1;

		view->entries[i].other = calloc(ID_LENGTH + 1, 1);
		if (!view->entries[i].other) break;
		memcpy(view->entries[i].other, V(id));

		i++;
	}

	for (i = 0; i < view->length; i++)
		free(view->entries[i].other);
	free(view->entries);
	close(fd);
	return -1;
}

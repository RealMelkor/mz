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
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include "termbox.h"
#include "view.h"
#include "file.h"
#include "strlcpy.h"
#include "client.h"
#include "util.h"

int file_init(struct view *view, const char* path) {

	PZERO(view);

	if (path)
		sstrcpy(view->path, path);
	else if (view->path != getcwd(V(view->path)))
		return -1;

	view->fd = open(view->path, O_DIRECTORY);
	if (view->fd < 0)
		return -1;

	return 0;
}

int file_up(struct view *view) {
	return file_cd(view, "..");
}

int file_cd(struct view *view, const char *path) {

	char buf[PATH_MAX];
	int fd, len, back;

	if (!strcmp(path, ".")) return 0;

	len = sstrcpy(buf, view->path);
	if (!strcmp(path, "..")) {
		if (!strcmp(view->path, "/")) {
			return 0;
		}
		len--;
		while (len >= 0) {
			if (buf[len] == '/') {
				buf[AZ(len)] = '\0';
				back = 1;
				break;
			}
			len--;
		}
		if (!back) return 0;
	} else {
		if (buf[AZ(len) - 1] != '/') {
			buf[len] = '/';
			len++;
		}
		strlcpy(&buf[len], path, sizeof(buf) - len);
	}

	fd = open(buf, O_DIRECTORY);
	if (fd < 0) return -1;
	close(view->fd);
	view->fd = fd;
	fchdir(fd);
	sstrcpy(view->path, buf);
	view->selected = 0;
	return 0;
}

void file_free(struct view *view) {
	free(view->entries);
	view->length = 0;
	view->entries = NULL;
}

int sort(const void* a, const void* b)
{
	struct entry *first = (struct entry*)a;
	struct entry *second = (struct entry*)b;
	int i = 0, j = 0;

	if (first->type != second->type) {
		if (first->type == DT_DIR) return -1;
		if (second->type == DT_DIR) return 1;
	}
	while (1) {
		uint32_t c1, c2;
		do {
			i += tb_utf8_char_to_unicode(&c1, &first->name[i]);
			if (!first->name[i]) return 0;
		} while (c1 == ' ' || c1 == '\t');
		do {
			j += tb_utf8_char_to_unicode(&c2, &second->name[j]);
			if (!second->name[j]) return 0;
		} while (c2 == ' ' || c2 == '\t');
		c1 = tolower(c1);
		c2 = tolower(c2);
		if (c1 != c2)
			return c1 < c2 ? -1 : 1;
	}
	return 0;
}

int file_reload(struct view *view) {
	close(view->fd);
	view->fd = open(view->path, O_DIRECTORY);
	if (view->fd < 0) return -1;
	return file_ls(view);
}

int file_ls(struct view *view) {
	struct dirent *entry;
	DIR *dp;
	int length, i, fd;

	fd = dup(view->fd);
	dp = fdopendir(fd);
	if (dp == NULL)
		return -1;

	length = 0;
	while ((entry = readdir(dp))) {
		if (!view->showhidden && entry->d_name[0] == '.')
			continue;
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
			length++;
	}
	if (length == 0) {
		closedir(dp);
		file_free(view);
		return 0;
	}
	
	rewinddir(dp);
	file_free(view);
	view->entries = malloc(sizeof(struct entry) * length);
	i = 0;
	while ((entry = readdir(dp))) {
		if (!strcmp(entry->d_name, ".") ||
		    !strcmp(entry->d_name, ".."))
			continue;
		if (!view->showhidden && entry->d_name[0] == '.')
			continue;
		view->entries[i].selected = 0;
		strlcpy(view->entries[i].name, entry->d_name,
			sizeof(view->entries[i].name));

#ifndef sun
		if (entry->d_type == DT_LNK) {
#endif
			struct stat buf;
			view->entries[i].type =
				fstatat(view->fd, entry->d_name, &buf, 0);
			view->entries[i].type = S_ISDIR(buf.st_mode) ?
				DT_DIR : DT_REG;
#ifndef sun
		} else
			view->entries[i].type = entry->d_type;
#endif
		i++;
	}

	qsort(view->entries, length, sizeof(struct entry), sort);

	closedir(dp);
	close(fd);
	view->length = length;
	view->scroll = 0;
	lseek(view->fd, 0, SEEK_SET);
	return 0;
}

int file_select(struct view *view, const char *path) {
	size_t i = 0;
	while (i < view->length) {
		if (!strncmp(view->entries[i].name, path,
			     sizeof(view->entries[i].name))) {
			view->selected = i;
			return 0;
		}
		i++;
	}
	return -1;
}

int file_move(struct view *view, struct entry *entry) {

	int fd, ret;

	fd = openat(view->fd, entry->name, 0);
	if (fd > -1) {
		errno = EEXIST;
		close(fd);
		return -1;
	}

	fd = open(client.copy_path, O_DIRECTORY);
	if (fd < 0) return -1;

	ret = renameat(fd, entry->name, view->fd, entry->name);
	close(fd);
	if (ret) return -1;

	return 0;
}

int file_copy(struct view *view, struct entry *entry) {

	struct stat st;
	int fd, dstfd, srcfd;
	off64_t ret, length;

	fd = openat(view->fd, entry->name, 0);
	if (fd > -1) {
		close(fd);
		errno = EEXIST;
		return -1;
	}

	fd = open(client.copy_path, O_DIRECTORY);
	if (fd < 0) return -1;
	srcfd = openat(fd, entry->name, O_RDONLY);
	close(fd);
	if (srcfd < 0) return -1;

	if (fstat(srcfd, &st)) {
		close(srcfd);
		return -1;
	}

	dstfd = openat(view->fd, entry->name, O_WRONLY|O_CREAT, st.st_mode);
	if (dstfd < 0) return -1;

	length = lseek64(srcfd, 0, SEEK_END);
	if (length == (off64_t)-1 ||
			lseek64(srcfd, 0, SEEK_SET) == (off64_t)-1) {
		close(dstfd);
		close(srcfd);
		return -1;
	}
	ret = sendfile(dstfd, srcfd, NULL, length);
	close(dstfd);
	close(srcfd);
	if (ret != length) return -1;

	return 0;
}

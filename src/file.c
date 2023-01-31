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
#include <sys/stat.h>
#include "termbox.h"
#include "view.h"
#include "file.h"
#include "strlcpy.h"
#include "client.h"
#include "util.h"

int file_init(struct view *view) {

	PZERO(view);
	if (view->path != getcwd(V(view->path)))
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
	int fd = openat(view->fd, path, O_DIRECTORY);
	if (fd < 0) return -1;
	close(view->fd);
	view->fd = fd;
	fchdir(fd);
	getcwd(V(view->path));
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

int file_ls(struct view *view) {
	struct dirent *entry;
	DIR *dp;
	int length, i, fd;

	fd = dup(view->fd);
	dp = fdopendir(fd);
	if (dp == NULL)
		return -1;

	/* make a array of entry, sort it */
	length = 0;
	while ((entry = readdir(dp))) {
		if (!view->showhidden && entry->d_name[0] == '.')
			continue;
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
			length++;
	}
	if (length == 0) {
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
		if (entry->d_type == DT_LNK) {
			struct stat buf;
			view->entries[i].type =
				fstatat(view->fd, entry->d_name, &buf, 0);
			view->entries[i].type = S_ISDIR(buf.st_mode) ?
				DT_DIR : DT_REG;
		} else
			view->entries[i].type = entry->d_type;
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

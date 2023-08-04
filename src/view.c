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
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "termbox.h"
#include "client.h"
#include "view.h"
#include "file.h"
#include "util.h"
#include "strlcpy.h"
#include "utf8.h"
#include "trash.h"

struct view *view_init(const char *path) {
	struct view *view = malloc(sizeof(struct view));
	file_init(view, path);
	return view;
}

void view_open(struct view *view) {

	char buf[2048];

	if (view->length < 1)
		return;

	client.error = 0;
	switch (view->entries[view->selected].type) {
	case DT_REG:
		if (view->fd == TRASH_FD) break;
		chdir(view->path);
		if ((size_t)snprintf(V(buf), "xdg-open \"%s\" >/dev/null 2>&1 &",
			  view->entries[view->selected].name) >= sizeof(buf)) {
			sstrcpy(client.info, "path too long");
			client.error = 1;
			break;
		}
		system(buf);
		break;
	case DT_DIR:
		if (file_cd(view, view->entries[view->selected].name)) {
			sstrcpy(client.info, strerror(errno));
			client.error = 1;
			break;
		}
		file_ls(view);
		break;
	}
}

void view_draw(struct view *view) {

	size_t i = 0, start = TABS;

	if (view->selected + 1 > view->scroll + HEIGHT)
		view->scroll = view->selected - HEIGHT;
	if (view->selected < view->scroll)
		view->scroll = view->selected;

	while (i + view->scroll < view->length) {
		int selected;
		struct entry *e;
		uintattr_t fg, bg;

		if (i > HEIGHT)
			break;

		fg = bg = TB_DEFAULT;

		selected = view->selected == i + view->scroll;
		e = &view->entries[i + view->scroll];
		if (selected) {
			fg = TB_WHITE;
			bg = TB_CYAN;
		}
		if (e->type == DT_DIR)
			fg = selected ? TB_BLACK : TB_CYAN;
		if (e->selected)
			bg = TB_CYAN;
		if (e->selected && fg == TB_CYAN)
			fg = TB_WHITE;
		if (e->selected && selected)
			fg = e->type == DT_REG ? TB_BLACK : TB_GREEN;
		tb_print(0, i + start, fg, bg, e->name);
		if (e->type == DT_DIR) {
			tb_set_cell(utf8_width(e->name, PATH_MAX), i + start,
					'/', fg, bg);
		}
		i++;
	}
}

void view_select(struct view *view, const char *name) {
	file_select(view, name);
	if (view->length < HEIGHT) return;
	if (view->selected >= view->length - HEIGHT / 2) {
		view->scroll = view->length - HEIGHT - 1;
	} else if (view->selected > HEIGHT / 2) {
		view->scroll = view->selected - HEIGHT / 2;
	}
}

void view_unselect(struct view *view) {
	size_t i = 0;
	while (i < view->length) {
		view->entries[i].selected = 0;
		i++;
	}
}

/*
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
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

struct view {
	unsigned int scroll;
	unsigned int selected;
	int fd;
	char path[1024];
	struct entry *entries;
	size_t length;
	int showhidden;
	int size;
	struct view *next;
	struct view *prev;
};

struct view *view_init(const char *path);
void view_open(struct view *view);
void view_draw(struct view *view);
void view_select(struct view *view, const char *name);
void view_unselect(struct view *view);

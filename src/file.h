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

struct entry {
	char name[1024];
	int type;
	int selected;
	void *other; /* custom data for non-regular entry */
};

int file_init(struct view* view, const char *path);
int file_ls(struct view *view);
int file_reload(struct view *view);
int file_cd(struct view *view, const char *path);
int file_up(struct view *view);
int file_select(struct view *view, const char *path);
int file_move_entry(struct view *view, struct entry *entry);
int file_move(const char *oldpath, int srcdir, const char *oldname,
		int dstdir, const char *newpath, const char *newname);
int file_copy(int src, int dst, int usebuf);
int file_copy_entry(struct view *view, struct entry *entry);
void file_free(struct view *view);
int file_sort(const void* a, const void* b);
int file_is_directory(const char *path);
int file_cd_abs(struct view *view, const char *path);

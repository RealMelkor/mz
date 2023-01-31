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

#define TABS (client.view->next || client.view->prev)
#define HEIGHT (TABS ? (client.height - 4) : (client.height - 3))

enum {
	MODE_NORMAL,
	MODE_COMMAND,
	MODE_SEARCH,
	MODE_VISUAL
};

struct client {
	struct view *view;
	size_t width;
	size_t height;
	int counter;
	int mode;
	int error;
	int g;
	int y;
	char field[1024];
	char info[1024];
	char search[1024];
};
extern struct client client;

int client_init();
int client_clean();
int client_update();
int client_input();

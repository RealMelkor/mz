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
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "termbox.h"
#include "view.h"
#include "client.h"
#include "file.h"
#include "strlcpy.h"
#include "wcwidth.h"
#include "utf8.h"
#include "trash.h"
#include "util.h"

#define TAB_WIDTH_LIMIT 20

struct client client;

static int display_tab(struct view *view, int x) {
	char *ptr = strrchr(view->path, '/');
	size_t length;
	char buf[1024];
	if (!ptr) return -1;
	ptr++;
	length = AZ(utf8_width(ptr, sizeof(view->path) - (ptr - view->path)));
	if (length > TAB_WIDTH_LIMIT) {
		length = TAB_WIDTH_LIMIT;
		strlcpy(buf, ptr, length + 1); /* sizeof buf > length */
		ptr = buf;
	}
	tb_printf(x, 0, (client.view == view ? TB_DEFAULT : TB_BLACK),
		  (client.view == view ? TB_DEFAULT : TB_WHITE), " %s ", ptr);
	return length + 2;
}

static int name_length(struct view *view) {
	char *ptr = strrchr(view->path, '/');
	if (!ptr) return -1;
	ptr++;
	return MAX(AZ(utf8_width(ptr, sizeof(view->path) - (ptr - view->path))),
		TAB_WIDTH_LIMIT);
}

int client_init() {

	char *env;

	PZERO(&client);

	client.view = view_init(getenv("PWD"));
	if (!client.view || file_ls(client.view)) return -1;

	env = getenv("HOME");
	if (!env) {
		printf("home folder not found\n");
		return -1;
	}

	client.trash = trash_init();
	if (client.trash < 0) return -1;

	if (tb_init()) return -1;
	client.width = tb_width();
	client.height = tb_height();

	return 0;
}

int client_clean() {
	free(client.copy);
	free(client.view);
	return tb_shutdown();
}

static void client_tabbar(struct view *view) {
	struct view *ptr, *start;
	int x, width, prev_sum, next_sum;

	width = client.width - name_length(view) - 2;
	if (width <= 0) {
		start = view;
		goto draw;
	}

	ptr = view;
	if (!ptr->prev) {
		start = ptr;
		goto draw;
	}
	prev_sum = 0;
	while (ptr->prev) {
		ptr = ptr->prev;
		prev_sum += name_length(ptr) + 2;
	}

	if (prev_sum < width/2) {
		start = ptr;
		goto draw;
	}

	ptr = view;
	next_sum = 0;
	while (ptr->next) {
		ptr = ptr->next;
		next_sum += name_length(ptr) + 2;
	}

	if (next_sum < width/2) {
		x = client.width;
		while (ptr) {
			int len = name_length(ptr) + 2;
			if (x - len < 0) {
				ptr = ptr->next;
				break;
			}
			x -= len;
			if (!ptr->prev) break;
			ptr = ptr->prev;
		}
		start = ptr;
		goto draw;
	}

	ptr = view;
	x = width/2;
	while (ptr) {
		int len = name_length(ptr) + 2;
		if (x - len < 0) {
			ptr = ptr->next;
			break;
		}
		x -= len;
		if (!ptr->prev) break;
		ptr = ptr->prev;
	}
	start = ptr;
draw:
	ptr = start;
	x = 0;
	while (ptr && x < (signed)client.width) {
		x += display_tab(ptr, x);
		ptr = ptr->next;
	}

	while (x < (signed)client.width) {
		tb_set_cell(x, 0, ' ', TB_DEFAULT, TB_WHITE);
		x++;
	}
	return;
}

int client_update() {

        char counter[32];
        struct view *view = client.view;
	size_t i;

	tb_clear();

	/* display list view */
        view_draw(view);

	/* display input field, error and counter */
        tb_print(0, client.height - 1, TB_DEFAULT,
                client.error ? TB_RED : TB_DEFAULT,
                client.error ? client.info : client.field);

        snprintf(V(counter), "%d", client.counter);
        if (client.counter)
                tb_print(client.width - 8, client.height - 1,
                        TB_DEFAULT, TB_DEFAULT, counter);

	/* display white status bar */
	i = 0;
	while (i < client.width) {
		tb_set_cell(i, client.height - 2, ' ', TB_BLACK, TB_WHITE);
		i++;
	}
        tb_print(0, client.height - 2, TB_BLACK, TB_WHITE, view->path);


	/* display tabs bar if there's more than one tab */
	if (TABS)
		client_tabbar(view);

	tb_present();

        if (client_input()) return 1;
        return 0;
}

static int newtab() {
	struct view *new;

	chdir(client.view->path);
	new = view_init(client.view->path);

	if (!new || file_ls(new)) {
		snprintf(V(client.info), "%s", strerror(errno));
		return -1;
	}
	if (client.view->next) {
		new->next = client.view->next;
		client.view->next->prev = new;
	}
	new->prev = client.view;
	client.view->next = new;
	new->selected = client.view->selected;
	client.view = client.view->next;
	return 0;
}

static int closetab() {

	struct view *view = client.view;

	if (!view) return -1;
	client.view = NULL;
	if (view->prev) {
		view->prev->next = view->next;
		client.view = view->prev;
	}
	if (view->next) {
		view->next->prev = view->prev;
		client.view = view->next;
	}
	if (client.view && file_ls(client.view)) {
		snprintf(V(client.info), "%s", strerror(errno));
		return -1;
	}

	close(view->fd);
	file_free(view);
	free(view);
	return client.view == NULL;
}

int parse_command() {

        char *cmd = &client.field[1];

        if (cmd[0] == 'q' && cmd[1] == '\0')
                return closetab();
        if (cmd[0] == 'q' && cmd[1] == 'a' && cmd[2] == '\0') {
                while (!closetab()) ;
                return 1;
        }
        if ((cmd[0] == 'n' && cmd[1] == 't' && cmd[2] == '\0') ||
		!strncmp(cmd, "tabnew", sizeof(client.field) - 1))
                return newtab();
	if (cmd[0] == '!') {
		tb_shutdown();
		if (system(&cmd[1])) sleep(1);
		tb_init();
		file_ls(client.view);
		return 0;
	}
	if (!strncmp(cmd, "sh", sizeof(client.field) - 1)) {
		tb_shutdown();
		system("$SHELL");
		tb_init();
		file_ls(client.view);
		return 0;
	}

        snprintf(V(client.info), "Not a command: %s", cmd);
        client.error = 1;
        return 0;
}

static void client_select(int next) {
	
	struct view *view = client.view;
	size_t i;

	if (view->length < 1 || !*client.search)
		return;

	i = view->selected + next;
	while (i != view->selected || !next) {
		if (!next) next = 1;
		if (i == view->length) i = 0;
		if (strcasestr(view->entries[i].name, client.search)) {
			view->selected = i;
			break;
		}
		if (next < 0 && i == 0) i = view->length;
		i += next;
	}
}

int client_command(struct tb_event ev) {

        int pos;

        switch (ev.key) {
        case TB_KEY_ESC:
                client.mode = MODE_NORMAL;
		client.field[0] = '\0';
                return 0;
        case TB_KEY_BACKSPACE2:
        case TB_KEY_BACKSPACE:
                pos = utf8_len(V(client.field));
                if (pos > 1)
                        client.field[pos - utf8_last_len(V(client.field))] = 0;
                else {
                        client.mode = MODE_NORMAL;
			client.field[0] = '\0';
		}
                return 0;
        case TB_KEY_ENTER:
		pos = 0;
		if (client.mode == MODE_COMMAND)
                	pos = parse_command();
                client.mode = MODE_NORMAL;
                client.field[0] = '\0';
                return pos;
        }

        if (!ev.ch) return 0;

        pos = utf8_len(client.field, sizeof(client.field) - 2);
	pos += tb_utf8_unicode_to_char(&client.field[pos], ev.ch);
        client.field[pos] = '\0';
	if (client.mode == MODE_SEARCH) {
		strlcpy(client.search, &client.field[1],
			sizeof(client.search) - 2);
		client_select(0);
	}

        return 0;
}

void client_unselect(struct view *view) {
	size_t i = 0;
	while (i < view->length) {
		view->entries[i].selected = 0;
		i++;
	}
}

void client_reset() {
	client.counter = client.g = client.y = 0;
}

int client_input() {
	struct tb_event ev;
	struct view *view = client.view;
	size_t i = 0;
	int ret;

	ret = tb_poll_event(&ev);
	if (ret != TB_OK && ret != TB_ERR_POLL) {
		return -1;
	}

	if (ev.type == TB_EVENT_RESIZE) {
		client.width = ev.w;
		client.height = ev.h;
		return 0;
	}

	if (ev.type != TB_EVENT_KEY) {
		return 0;
	}

	if (client.mode == MODE_COMMAND || client.mode == MODE_SEARCH) {
		return client_command(ev);
	}

	if (ev.key == TB_KEY_ESC) {
		client_reset();
		return 0;
	}

	if (ev.ch >= (client.counter ? '0' : '1') && ev.ch <= '9') {
                int i = ev.ch - '0';
		client.g = 0;
                if (client.counter >= 100000000) return 0;
                if (client.counter == 0 && i == 0) return 0;
                client.counter = client.counter * 10 + i;
                return 0;
        }
	
	if (ev.key == TB_KEY_ENTER) goto open;

	switch (ev.ch) {
	case 'j':
		ADDMAX(view->selected, AZ(client.counter), view->length - 1);
		client.counter = 0;
		break;
	case 'k':
		SUBMIN(client.view->selected, AZ(client.counter), 0);
		client.counter = 0;
		break;
	case 'l':
open:
		view_open(view);
		break;
	case 'h':
	{
		char name[1024];
		char *ptr = strrchr(view->path, '/');
		if (!ptr)
			break;
		sstrcpy(name, ptr + 1);
		if (file_up(view))
			break;
		file_ls(view);
		view_select(view, name);
	}
		break;
	case 'T':
	case 't':
		if (!client.g) break;
		if (ev.ch == 'T') { /* backward */
			if (view->prev) client.view = view->prev;
			else {
				while (view->next) view = view->next;
				client.view = view;
			}
		} else { /* forward */
			if (view->next) client.view = view->next;
			else {
				while (view->prev) view = view->prev;
				client.view = view;
			}
		}
		file_ls(client.view);
		client_reset();
		break;
	case '.':
		TOGGLE(view->showhidden);
		file_ls(view);
		break;
	case '/': /* search */
	case ':': /* command */
		client.mode = ev.ch == '/' ? MODE_SEARCH: MODE_COMMAND;
		client.error = 0;
		client_reset();
		client.field[0] = ev.ch;
                client.field[1] = '\0';
		break;
	case 'n': /* next occurence */
		client_select(1);
		break;
	case 'N': /* previous occurence */
		client_select(-1);
		break;
	case 'e':
		if (SELECTED(view).type == DT_DIR)
			break;
	{
		char buf[2048];
		tb_shutdown();
		snprintf(V(buf), "$EDITOR \"%s/%s\"",
				view->path, SELECTED(view).name);
		system(buf);
		tb_init();
	}
		break;
	case 'G':
		view->selected = view->length - 1;
		break;
	case 'g':
		if (client.g) {
			view->selected = 0;
			client.g = 0;
			break;
		}
		client.g = 1;
		break;
	case 'd': /* delete (move to trash) */
	{
		size_t i = 0;
		while (i < view->length) {
			if (!view->entries[i].selected) {
				i++;
				continue;
			}
			if (trash_send(view->fd, view->path,
					view->entries[i].name)) {
				client.error = 1;
				sstrcpy(client.info, strerror(errno));
				break;
			}
			view->entries[i].selected = 0;
			i++;
		}
	}
		file_ls(view);
		break;
	case 'p': /* paste */
		if (!client.copy_length) break;
		i = 0;
		while (i < client.copy_length) {
			if (client.cut ? file_move(view, &client.copy[i]) :
					file_copy(view, &client.copy[i])) {
				client.error = 1;
				sstrcpy(client.info, strerror(errno));
				break;
			}
			i++;
		}
		free(client.copy);
		client.copy = NULL;
		client.copy_length = 0;
		file_reload(view);
		break;
	case 'x': /* cut */
	case 'c': /* copy */
	{
		size_t i = 0, j = 0, length = 0;
		while (i < view->length) {
			if (view->entries[i].selected) length++;
			i++;
		}
		if (!length) break;
		free(client.copy);
		client.copy = malloc(sizeof(struct entry) * length);
		sstrcpy(client.copy_path, view->path);
		client.copy_length = length;
		i = 0;
		while (i < view->length) {
			if (view->entries[i].selected) {
				client.copy[j] = view->entries[i];
				view->entries[i].selected = 0;
				j++;
			}
			i++;
		}
		client.cut = ev.ch == 'x';
	}
		break;
	case 'y': /* copy selection path to clipboard */
		if (!client.y) {
			client.y = 1;
			break;
		}
	{
		char buf[2048];
		snprintf(V(buf),
			"echo \"%s/%s\" | xclip -sel clip 2>/dev/null",
			view->path, SELECTED(view).name);
		if (system(buf) && getenv("TMUX")) { /* try tmux if no xclip */
			snprintf(V(buf), "echo \"%s/%s\" | tmux load-buffer -",
				view->path, SELECTED(view).name);
			system(buf);
		}
	}
		client.y = 0;
		break;
	case ' ': /* select */
		TOGGLE(SELECTED(view).selected);
		break;
	}

	return 0;
}

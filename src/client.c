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
#include <stdlib.h>
#include "termbox.h"
#include "view.h"
#include "client.h"
#include "file.h"
#include "strlcpy.h"
#include "util.h"

struct client client;

static char *fetch_name(struct view *view) {
	char *ptr = strrchr(view->path, '/');
	if (!ptr) return NULL;
	return ptr + 1;
}

static int name_length(struct view *view) {
	char *ptr = strrchr(view->path, '/');
	if (!ptr) return -1;
	ptr++;
	return strnlen(ptr, sizeof(view->path) - (ptr - view->path));
}

int client_init() {
	client.view = view_init();
	if (!client.view || file_ls(client.view)) return -1;
	if (tb_init()) return -1;
	client.width = tb_width();
	client.height = tb_height();
	return 0;
}

int client_clean() {
	free(client.view);
	return tb_shutdown();
}

static void client_tabbar(struct view *view) {
	size_t sum_next, sum_prev;
	struct view *ptr = view;

	sum_next = 0;
	while (ptr) {
		sum_next += name_length(ptr) + 2;
		ptr = ptr->next;
	}

	ptr = view->prev;
	sum_prev = 0;
	while (ptr) {
		sum_prev += name_length(ptr) + 2;
		if (!ptr->prev) break;
		ptr = ptr->prev;
	}
	if (!ptr) ptr = view;

	/* most likely scenario */
	if (sum_prev + sum_next < (unsigned)client.width) {
		size_t x = 0;
		while (ptr) {
			tb_printf(x, 0,
				(ptr == view ? TB_DEFAULT : TB_UNDERLINE),
				(ptr == view ? TB_DEFAULT : TB_WHITE),
				" %s ", fetch_name(ptr));
			x += name_length(ptr) + 2;
			ptr = ptr->next;
		}
		while (x < (unsigned)client.width) {
			tb_set_cell(x, 0, ' ', TB_DEFAULT, TB_WHITE);
			x++;
		}
	} else if (sum_prev > client.width/2 && sum_next > client.width/2) {
		/* draw to the left till it reach half width
		 * then start drawing to the right till it full width */
	}

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
	if (TABS) {
		/*
		 * count the sum of file name length on prev tabs and the sum
		 * of those on next tabs,
		 * - if the sum of both sums is less than the client width than 
		 *   print all from x:0
		 * - else if the sum of the next tabs is less than half width 
		 *   but the sum of the prev tabs is more than half width than
		 *   start by print from the last tab on the right
		 *
		 *   (the current tab is counted as a next tab)
		 */
		client_tabbar(view);
	}

	tb_present();

        if (client_input()) return 1;
        return 0;
}

static int newtab() {
	struct view *new = view_init();

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

        snprintf(V(client.info), "Not a command: %s", cmd);
        client.error = 1;
        return 0;
}

int client_command(struct tb_event ev) {

        int pos;

        switch (ev.key) {
        case TB_KEY_ESC:
                client.command = 0;
		client.field[0] = '\0';
                return 0;
        case TB_KEY_BACKSPACE2:
        case TB_KEY_BACKSPACE:
                pos = strnlen(client.field, sizeof(client.field));
                if (pos > 1)
                        client.field[pos - 1] = '\0';
                else
                        client.command = 0;
                return 0;
        case TB_KEY_ENTER:
                pos = parse_command();
                client.command = 0;
                client.field[0] = '\0';
                return pos;
        }

        if (!ev.ch) return 0;

        pos = strnlen(client.field, sizeof(client.field));
        client.field[pos] = ev.ch;
        client.field[pos + 1] = '\0';

        return 0;
}

void client_reset() {
	client.counter = client.g = 0;
}

int client_input() {
	struct tb_event ev;
	struct view *view = client.view;

	if (tb_poll_event(&ev) != TB_OK) {
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

	if (client.command) {
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
		view->showhidden = !view->showhidden;
		file_ls(view);
		break;
	case ':':
		client.command = 1;
		client.error = 0;
		client_reset();
		client.field[0] = ':';
                client.field[1] = '\0';
		break;
	case 'e':
	{
		char buf[1024];
		tb_shutdown();
		snprintf(V(buf), "$EDITOR %s/%s", view->path, SELECTED(view));
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
	}

	return 0;
}

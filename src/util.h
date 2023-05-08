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
#define V(X) X, sizeof(X) /* the value and its size */
#define VP(X) X, sizeof(*X) /* the pointer and its value size */
/* Load an xpm image */
#define IMAGE(X, Y) backend_load_image(X##_xpm, X##_width, X##_height, \
				       X##_colors, X##_char, Y)
#define SELECTED(X) X->entries[X->selected]
#define EMPTY(X) (X->selected < 0 || X->selected >= X->length)

/* Assign the sum of X and Y to X if the sum is lesser than Z, else assign Z */
#define ADDMAX(X, Y, Z) X = ((X + Y) > Z ? Z : (X + Y))
/* Assign the subtraction of Y and X to X if the subtraction is lesser than Z,
 * else assign Z */
#define SUBMIN(X, Y, Z) X = ((signed)(X - Y) < Z ? Z : (X - Y))
#define AZ(X) (X < 1 ? 1 : X) /* if X is above zero return X else return 1 */

#define RZERO(X) memset(&X, 0, sizeof(X)) /* Zero'd a reference */
#define PZERO(X) memset(X, 0, sizeof(*X)) /* Zero'd a pointer */

#define rev32(num) ( ((num & 0xFF000000) >> 24) | ((num & 0x00FF0000) >> 8) | \
                     ((num & 0x0000FF00) << 8) | ((num & 0x000000FF) << 24) )
#define rev24(num) ( (num & 0xFF000000) | ((num & 0x00FF0000) >> 16) | \
                     (num & 0x0000FF00) | ((num & 0x000000FF) << 16) )

#define INN(X, Y) X ? X : Y /* If not null return X else return Y */
#define TOGGLE(X) X = !X
#define MAX(X, Y) X > Y ? Y : X /* if X is greater than Y return Y */

#ifndef DT_REG
#define DT_REG 0
#endif

#ifndef DT_DIR
#define DT_DIR 1
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

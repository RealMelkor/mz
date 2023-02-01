#include <stdlib.h>
#include <stdint.h>
#include "termbox.h"
#include "wcwidth.h"
#include "utf8.h"

int utf8_width(char* ptr, size_t len) {
        int width = 0;
        char* max = ptr + len;
        while (*ptr && ptr < max) {
                uint32_t c;
                ptr += tb_utf8_char_to_unicode(&c, ptr);
                width += mk_wcwidth(c);
        }
        return width;
}

int utf8_len(char* ptr, size_t len) {
        char* max = ptr + len;
	char* start = ptr;
        while (*ptr && ptr < max) {
		ptr += tb_utf8_char_length(*ptr);
        }
        return ptr - start;
}

int utf8_last_len(char *ptr, size_t len) {
        char* max = ptr + len;
	len = 1;
        while (*ptr && ptr < max) {
                len = tb_utf8_char_length(*ptr);
		ptr += len;
        }
        return len;
}

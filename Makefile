SHELL = /bin/sh

PREFIX = /usr/local
CFLAGS = -ansi -Wall -Wextra -std=c89 -pedantic -O2 -D_POSIX_C_SOURCE=200809L
CC = cc
LIBS = -s -lm

build: src/*
	${CC} ${CFLAGS} src/main.c src/file.c src/strlcpy.c \
	src/view.c src/client.c src/termbox.c \
	${INCLUDES} ${LIBSPATH} \
	-o mz ${LIBS} 

install:
	cp mz ${PREFIX}/bin/
	chmod 755 ${PREFIX}/bin/mz

uninstall:
	rm ${PREFIX}/bin/mz

clean:
	rm -f mz

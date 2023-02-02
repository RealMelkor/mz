SHELL = /bin/sh

CC=cc
PREFIX=/usr/local
CFLAGS=-ansi -Wall -Wextra -std=c89 -pedantic -O2
LIBS=-s -lm

# uncomment to build on Illumos
#CFLAGS=-Wall -Wextra -pedantic -O2 -Wformat-truncation=0
#CC=gcc

build: src/*
	${CC} ${CFLAGS} src/main.c src/file.c src/strlcpy.c src/trash.c \
	src/view.c src/client.c src/termbox.c src/wcwidth.c src/utf8.c \
	${INCLUDES} ${LIBSPATH} \
	-o mz ${LIBS} 

install:
	cp mz ${PREFIX}/bin/
	chmod 755 ${PREFIX}/bin/mz

uninstall:
	rm ${PREFIX}/bin/mz

clean:
	rm -f mz

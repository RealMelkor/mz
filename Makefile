SHELL = /bin/sh

CC=cc
PREFIX=/usr/local
CFLAGS=-Wall -Wextra -std=c89 -pedantic -O2 -D_POSIX_C_SOURCE=200809L
LIBS=-s -lm

# uncomment to build on Illumos
#CFLAGS=-Wall -Wextra -pedantic -O2 -Wformat-truncation=0
#CC=gcc

build: src/*
	${CC} ${CFLAGS} src/*.c ${INCLUDES} ${LIBSPATH} -o mz ${LIBS}

install:
	cp mz ${PREFIX}/bin/
	chmod 755 ${PREFIX}/bin/mz

uninstall:
	rm ${PREFIX}/bin/mz

clean:
	rm -f mz

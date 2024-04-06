SHELL = /bin/sh

CC=cc
PREFIX=/usr/local
CFLAGS=-ansi -Wall -Wextra -pedantic -O2
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

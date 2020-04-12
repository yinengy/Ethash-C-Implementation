CC=gcc
CFLAGS=-std=c99

all: lib/sha3.c ethash.c
	$(CC) -o cethash lib/sha3.c ethash.c $(CFLAGS)
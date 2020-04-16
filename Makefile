CC=gcc
CFLAGS=-std=c99 -lm

all: lib/sha3.c lib/mt19937-64.c ethash.c
	$(CC) -o cethash lib/sha3.c lib/mt19937-64.c ethash.c $(CFLAGS)

# variables
CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18
LOGIN = chencheny
SUBMITPATH = ~cs537-1/handin/chencheny/P3

# targets
.PHONY: all
all: wsh

wsh: wsh.c wsh.h
	$(CC) $(CFLAGS) $^ -o $@

run: wsh
	./wsh

pack:
	tar -czvf $(LOGIN).tar.gz wsh.h wsh.c Makefile README.md

submit:pack
	cp $(LOGIN).tar.gz $(SUBMITPATH)

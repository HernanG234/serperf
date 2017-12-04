.PHONY:clean all utils
CFLAGS=-Wall -Wextra -O99
SHELL=/bin/bash
NAME=serperf

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< -o $@

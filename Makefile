CFLAGS=-Wall -Wextra -O99 -Werror -Wno-unused-parameter -Wno-missing-field-initializers
SHELL=/bin/bash
NAME=serperf

$(NAME): serperf.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm serperf

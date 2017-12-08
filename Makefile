CFLAGS = -Wall -Wextra -O99 -Werror -Wno-unused-parameter -Wno-missing-field-initializers

serperf: serperf.o

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean
clean:
	rm -f serperf serperf.o

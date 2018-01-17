BINDIR=$(DESTDIR)/usr/bin
CFLAGS = -Wall -Wextra -O99 -Werror -Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS = -pthread

serperf: serperf.o

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $^ -o $@

.PHONY: install
install:
	install serperf $(BINDIR)/serperf

.PHONY: clean
clean:
	rm -f serperf serperf.o

CFLAGS ?= -O2 -Wall -Wextra
PREFIX ?= /usr/local
PKGS = gtk+-3.0 x11

CFLAGS += $(shell pkg-config --cflags $(PKGS))
LDLIBS = $(shell pkg-config --libs $(PKGS)) -lm

LIB_SRC = geometry.c output.c
LIB_HDR = geometry.h output.h

printa: printa.c $(LIB_SRC) $(LIB_HDR)
	$(CC) $(CFLAGS) -o $@ printa.c $(LIB_SRC) $(LDLIBS)

tests/test_printa: tests/test_printa.c $(LIB_SRC) $(LIB_HDR)
	$(CC) $(CFLAGS) -I. -o $@ tests/test_printa.c $(LIB_SRC) $(LDLIBS)

test: tests/test_printa
	./tests/test_printa

install: printa
	install -Dm755 printa $(DESTDIR)$(PREFIX)/bin/printa

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/printa

clean:
	rm -f printa tests/test_printa

.PHONY: test install uninstall clean

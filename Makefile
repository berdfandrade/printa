CFLAGS ?= -O2 -Wall -Wextra
PKGS = gtk+-3.0 x11

printa: printa.c
	$(CC) $(CFLAGS) -o $@ $< $$(pkg-config --cflags --libs $(PKGS)) -lm

clean:
	rm -f printa

.PHONY: clean

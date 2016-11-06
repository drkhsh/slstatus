##

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# compiler
CFLAGS = -std=c99 -Wno-unused-function -O0 -lX11

all: config sbar.c
	$(CC) $(CFLAGS) -o sbar sbar.c

clean:
	rm -f sbar

config:
	@if [ ! -f config.h ]; then\
		cp config.def.h config.h;\
	fi\

install: all
	cp sbar ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/sbar
	cp sbar.1 ${MANPREFIX}/man1
	chmod 644 ${MANPREFIX}/man1/sbar.1

uninstall:
	rm -f ${PREFIX}/bin/sbar
	rm -f ${MANPREFIX}/man1/sbar.1

.PHONY: all clean install uninstall
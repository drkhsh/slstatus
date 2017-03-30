# See LICENSE file for copyright and license details.

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11

CPPFLAGS = -D_GNU_SOURCE
# -Wno-unused-function for routines not activated by user
CFLAGS = -std=c99 -pedantic -Wno-unused-function -Wall -Wextra -Os -fstack-protector-strong -fstack-check -fPIE ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

CC = cc
LD = ld

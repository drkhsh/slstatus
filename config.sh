NAME="slstatus"
VERSION="1.0"

PREFIX="/usr/local"

X11INC="/usr/X11R6/include"
X11LIB="/usr/X11R6/lib"

INCS="-I. -I/usr/include -I${X11INC}"
LIBS="-L/usr/lib -lc -L${X11LIB} -lX11 -lasound"

CPPFLAGS="-DVERSION=\"${VERSION}\" -D_GNU_SOURCE"
CFLAGS="-std=c99 -pedantic -Wno-unused-function -Wall -Wextra -O0 ${INCS} ${CPPFLAGS}"
LDFLAGS="${LIBS}"

CC = cc


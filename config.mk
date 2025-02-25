# slstatus version
VERSION = 1.0

# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

X11INC != pkg-config --cflags x11
X11LIB != pkg-config --libs x11

# flags
CPPFLAGS = $(X11INC) -D_DEFAULT_SOURCE -DVERSION=\"${VERSION}\"
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter -Os
LDFLAGS  = $(X11LIB) -s
# OpenBSD: add -lsndio
# FreeBSD: add -lkvm -lsndio
LDLIBS   = -lasound

# compiler and linker
CC = cc

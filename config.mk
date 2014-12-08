PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

CFLAGS = -c -g -std=c99 -Wall

CFLAGS += $(shell pkg-config --cflags json-c)
LDFLAGS += $(shell pkg-config --libs json-c)

CC ?= gcc

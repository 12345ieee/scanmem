#
# $Id: Makefile,v 1.17 2007-04-09 00:03:49+01 taviso Exp $
#

# indent -kr -l80 -nut *.c *.h
SOURCES=main.c maps.c ptrace.c list.c menu.c commands.c handlers.c value.c
INCLUDES=scanmem.h list.h value.h interrupt.h handlers.h commands.h
DISTFILES=$(SOURCES) $(INCLUDES) Makefile scanmem.1 ChangeLog README TODO COPYING
VERSION=0.06
CC=gcc
CFLAGS=-W -Wall -O2 -g
CPPFLAGS=-DVERSIONSTRING="\"v$(VERSION)\""
LDFLAGS=-lreadline
PREFIX=/usr/local

all: scanmem

scanmem: $(SOURCES:.c=.o)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(SOURCES:.c=.o)

clean:
	-rm -rf $(SOURCES:.c=.o) scanmem *.core scanmem-$(VERSION)* scanmem.1.gz

install: scanmem
	gzip -c scanmem.1 > scanmem.1.gz
	install -D --mode=0755 scanmem $(PREFIX)/bin/scanmem
	install -D --mode=0644 scanmem.1.gz $(PREFIX)/share/man/man1/scanmem.1.gz
	
dist: clean
	mkdir scanmem-$(VERSION)
	cp $(DISTFILES) scanmem-$(VERSION)
	tar --mode=u+wr,go+r -zcvf scanmem-$(VERSION).tar.gz scanmem-$(VERSION)
	@ls -l scanmem-$(VERSION).tar.gz

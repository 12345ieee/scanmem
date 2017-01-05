#
# $Id: Makefile,v 1.8 2007-03-03 13:25:56+00 taviso Exp $
#

SOURCES=main.c maps.c ptrace.c list.c menu.c
INCLUDES=scanmem.h list.h
DISTFILES=$(SOURCES) $(INCLUDES) Makefile scanmem.1 ChangeLog README TODO
VERSION=0.05
CC=gcc
CFLAGS=-W -Wall -O2 -DVERSIONSTRING="\"v$(VERSION)\""
LDFLAGS=-lreadline -lm
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
	tar -zcvf scanmem-$(VERSION).tar.gz scanmem-$(VERSION)
	@ls -l scanmem-$(VERSION).tar.gz

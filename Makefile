#
# $Id: Makefile,v 1.4 2006-11-15 23:48:15+00 taviso Exp $
#

SOURCES=main.c maps.c ptrace.c list.c menu.c
INCLUDES=scanmem.h list.h
DISTFILES=$(SOURCES) $(INCLUDES) Makefile scanmem.1 ChangeLog README TODO
VERSION=0.03
CC=gcc
CFLAGS=-W -Wall -O2 -DVERSIONSTRING="\"v$(VERSION)\""

all: scanmem

scanmem: $(SOURCES:.c=.o)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(SOURCES:.c=.o)

clean:
	-rm -rf $(SOURCES:.c=.o) scanmem *.core scanmem-$(VERSION)*

dist: clean
	mkdir scanmem-$(VERSION)
	cp $(DISTFILES) scanmem-$(VERSION)
	tar -zcvf scanmem-$(VERSION).tar.gz scanmem-$(VERSION)
	@ls -l scanmem-$(VERSION).tar.gz

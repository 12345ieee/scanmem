#
# $Id: Makefile,v 1.2 2006-11-14 11:17:34+00 taviso Exp taviso $
#

SOURCES=main.c maps.c ptrace.c
DISTFILES=$(SOURCES) scanmem.h Makefile scanmem.1 ChangeLog README TODO
CFLAGS=-W -Wall -O2
VERSION=0.02
CC=gcc

all: scanmem

scanmem: $(SOURCES:.c=.o)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(SOURCES:.c=.o)

clean:
	-rm -f $(SOURCES:.c=.o) scanmem *.core scanmem-*.tar.gz
	-rm -rf scanmem-$(VERSION)

dist: clean
	mkdir scanmem-$(VERSION)
	cp $(DISTFILES) scanmem-$(VERSION)
	tar -zcvf scanmem-$(VERSION).tar.gz scanmem-$(VERSION)
	@ls -l scanmem-$(VERSION).tar.gz

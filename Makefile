CC=gcc
CFLAGS=-O2 -Wall

scanmem: main.o maps.o ptrace.o
	$(CC) $(CFLAGS) -o scanmem main.o maps.o ptrace.o

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	-rm -f *.o scanmem *.core core

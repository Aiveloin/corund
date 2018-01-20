MAKEFLAGS=
CFLAGS=-march=native -O2 -pipe -Wall -Wextra -pedantic -std=c11
BINARY=corund
CC=gcc

all: main.o
	${CC} main.o -o ${BINARY} -lX11 -lpng && ./corund

main.o: main.c
	${CC} -mmusl -c ${CFLAGS} main.c

clean:
	rm -f *.o a.out

distclean: clean
	rm -f ${BINARY} 
MAKEFLAGS=
#CFLAGS=-march=native -O2 -pipe -Wall -Wextra -pedantic -std=c11
CFLAGS=-march=native -O2 -pipe
BINARY=corund
CC=gcc

all: main.o
	${CC} main.o -o ${BINARY} -lX11 -lpng

main.o:
	${CC} -mmusl -c ${CFLAGS} main.c

clean:
	rm -f *.o a.out

distclean: clean
	rm -f ${BINARY} 
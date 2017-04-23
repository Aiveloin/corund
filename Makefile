MAKEFLAGS=
CFLAGS=-march=native -O2 -pipe -Wall -Wextra -pedantic -std=c11
BINARY=corund
CC=musl-gcc

all: main.o
	${CC} main.o -o ${BINARY} -static

main.o:
	${CC} -c ${CFLAGS} main.c

clean:
	rm -f *.o

distclean: clean
	rm -f ${BINARY} 
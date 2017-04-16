MAKEFLAGS=
CFLAGS=-march=native -O2 -pipe -Wall -Wextra -pedantic -std=c11
BINARY=corund

all: main.o
	gcc main.o -o ${BINARY} -lSDL2

main.o:
	gcc -c ${CFLAGS} main.c

clean:
	rm -f *.o

distclean: clean
	rm ${BINARY} 
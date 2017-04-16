MAKEFLAGS=
CFLAGS=-march=native -O2 -pipe -Wall -Wextra -pedantic -std=c11
BINARY=corund

all: main.o
	gcc main.o -o ${BINARY}

main.o:
	gcc -c ${CFLAGS} main.c

clean:
	rm -f *.o

dist-clean: clean
	rm ${BINARY} 
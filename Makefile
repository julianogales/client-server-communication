run:
	gcc server.c -ansi -pedantic -Wall -o server
	./server
all:
	gcc server.c -ansi -pedantic -Wall -o server
clean:
	-rm -rf server

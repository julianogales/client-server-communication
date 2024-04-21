run:
	gcc server.c -ansi -pedantic -Wall -pthread -o server
	./server
all:
	gcc server.c -ansi -pedantic -Wall -pthread -o server
clean:
	-rm -rf server

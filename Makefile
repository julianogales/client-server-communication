run:
	gcc server.c -ansi -pedantic -Wall -pthread -o server
	./server
run debug:
	gcc server.c -ansi -pedantic -Wall -pthread -o server
	./server -d

all:
	gcc server.c -ansi -pedantic -Wall -pthread -o server
clean:
	-rm -rf server

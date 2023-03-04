all: server.out client.out



client.out: client.c
	gcc -g -Wall -o  client.out client.c

server.out: server.o logging.o
	gcc -g -Wall -o  server.out server.o logging.o
	

logging.o: logging.c
	gcc -g -Wall -c -o logging.o logging.c

server.o: server.c
	gcc -g -Wall -c -o server.o server.c

test.out: test.c
	gcc -g -Wall -o  test.out test.c

clean:
	rm *.o
	rm *.out


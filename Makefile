all: server.out client.out



client.out: client.c
	gcc -g -Wall -o  client.out client.c

server.out: server.c
	gcc -g -Wall -o  server.out server.c
	
test.out: test.c
	gcc -g -Wall -o  test.out test.c

clean:
	rm *.o
	rm *.out


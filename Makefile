CFLAGS = -Wall -g

PORT = 1233

IP_SERVER = 127.0.0.1

ID_SUBSCRIBER = 
build: server subscriber

server: server.c

subscriber: subscriber.c -lm

.PHONY: clean run_server run_subscriber

run_server:
	./server ${PORT}

run_subscriber:
	./client ${ID_SUBSCRIBER} ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber

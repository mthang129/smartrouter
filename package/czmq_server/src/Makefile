LDFLAGS += -lczmq
czmq_server: czmq_server.o
	$(CC) $(LDFLAGS) czmq_server.o -o czmq_server
czmq_server.o: czmq_server.c
	$(CC) $(CFLAGS) -c czmq_server.c

clean:
	rm *.o czmq_server

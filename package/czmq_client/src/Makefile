LDFLAGS += -lczmq -liwinfo
czmq_client: czmq_client.o iwinfo_cli.o
	$(CC) $(LDFLAGS) czmq_client.o iwinfo_cli.o -o czmq_client

czmq_client.o: czmq_client.c
	$(CC) $(CFLAGS) -c czmq_client.c

iwinfo_cli.o: iwinfo_cli.c
	$(CC) $(CFLAGS) -c iwinfo_cli.c
clean:
	rm *.o czmq_client

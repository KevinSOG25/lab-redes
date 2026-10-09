CC     = gcc
CFLAGS = -Wall

all: broker publisher subscriber

broker: broker_udp.c
	$(CC) $(CFLAGS) -o broker broker_udp.c

publisher: publisher_udp.c
	$(CC) $(CFLAGS) -o publisher publisher_udp.c

subscriber: subscriber_udp.c
	$(CC) $(CFLAGS) -o subscriber subscriber_udp.c

clean:
	rm -f broker publisher subscriber

.PHONY: all clean

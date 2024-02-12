CC = gcc
# CFLAGS = -Wall -Wextra -pedantic
SRC_COMMON = multicast.c zcs.c
OBJ_COMMON = $(SRC_COMMON:.c=.o)


all: app service app-2 service-2

app: app.c $(SRC_COMMON)
	$(CC) $(CFLAGS) -pthread -o $@ $^

service: service.c $(SRC_COMMON)
	$(CC) $(CFLAGS) -pthread -o $@ $^

app-2: app-2.c $(SRC_COMMON)
	$(CC) $(CFLAGS) -pthread -o $@ $^

service-2: service-2.c $(SRC_COMMON)
	$(CC) $(CFLAGS) -pthread -o $@ $^

clean:
	rm -f app service $(OBJ_COMMON)

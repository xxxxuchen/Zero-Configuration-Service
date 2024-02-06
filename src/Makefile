CC = gcc
# CFLAGS = -Wall -Wextra -pedantic
SRC_COMMON = multicast.c zcs.c
OBJ_COMMON = $(SRC_COMMON:.c=.o)


all: app service

app: app.c $(SRC_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

service: service.c $(SRC_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f app service $(OBJ_COMMON)

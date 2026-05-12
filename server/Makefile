CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -pthread -lrt
CC ?= $(CROSS_COMPILE)gcc

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f *.o aesdsocket

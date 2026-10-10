CFLAGS ?= -g -Wall -Werror -I../aesd-char-driver
LDFLAGS ?= -pthread -lrt
USE_AESD_CHAR_DEVICE ?= 1
CC ?= $(CROSS_COMPILE)gcc

ifneq ($(USE_AESD_CHAR_DEVICE), 0)
	CFLAGS += -DUSE_AESD_CHAR_DEVICE
endif

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f *.o aesdsocket

CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = -ldl -lpthread
LIB_LDFLAGS = -lwiringPi -lpthread

SERVER = server
LIBDIR = lib
LIB = $(LIBDIR)/libdevice.so

SERVER_SRCS = src/server.c src/daemon.c
DEVICE_SRCS = lib/led.c lib/buzzer.c lib/light.c lib/fnd.c

all: $(LIB) $(SERVER)

$(LIB): $(DEVICE_SRCS) include/device.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIB) $(DEVICE_SRCS) $(LIB_LDFLAGS)

$(SERVER): $(SERVER_SRCS) $(LIB)
	$(CC) $(CFLAGS) -o $(SERVER) $(SERVER_SRCS) $(LDFLAGS)

clean:
	rm -f $(SERVER) $(LIB)

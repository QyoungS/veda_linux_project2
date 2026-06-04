CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = -ldl -lpthread
LIB_LDFLAGS = -lwiringPi -lpthread

SERVER = server
LIBDIR = lib
LIBLED = $(LIBDIR)/libled.so
LIBBUZZER = $(LIBDIR)/libbuzzer.so
LIBLIGHT = $(LIBDIR)/liblight.so
LIBFND = $(LIBDIR)/libfnd.so
LIBS = $(LIBLED) $(LIBBUZZER) $(LIBLIGHT) $(LIBFND)

SERVER_SRCS = src/server.c src/daemon.c

all: $(LIBS) $(SERVER)

$(LIBLED): lib/led.c include/device.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBLED) lib/led.c $(LIB_LDFLAGS)

$(LIBBUZZER): lib/buzzer.c include/device.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBBUZZER) lib/buzzer.c $(LIB_LDFLAGS)

$(LIBLIGHT): lib/light.c include/device.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBLIGHT) lib/light.c $(LIB_LDFLAGS)

$(LIBFND): lib/fnd.c include/device.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBFND) lib/fnd.c $(LIB_LDFLAGS)

$(SERVER): $(SERVER_SRCS) $(LIBS)
	$(CC) $(CFLAGS) -o $(SERVER) $(SERVER_SRCS) $(LDFLAGS)

install: all
	mkdir -p ../../exec/lib
	cp $(SERVER) ../../exec/
	cp $(LIBLED) ../../exec/lib/
	cp $(LIBBUZZER) ../../exec/lib/
	cp $(LIBLIGHT) ../../exec/lib/
	cp $(LIBFND) ../../exec/lib/

clean:
	rm -f $(SERVER) $(LIBS)

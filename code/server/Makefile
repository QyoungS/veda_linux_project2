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

all: install

$(LIBLED): lib/led.c include/led.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBLED) lib/led.c $(LIB_LDFLAGS)

$(LIBBUZZER): lib/buzzer.c include/buzzer.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBBUZZER) lib/buzzer.c $(LIB_LDFLAGS)

$(LIBLIGHT): lib/light.c include/light.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBLIGHT) lib/light.c $(LIB_LDFLAGS)

$(LIBFND): lib/fnd.c include/fnd.h
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBFND) lib/fnd.c $(LIB_LDFLAGS)

$(SERVER): $(SERVER_SRCS) $(LIBS)
	$(CC) $(CFLAGS) -o $(SERVER) $(SERVER_SRCS) $(LDFLAGS)

install: $(LIBS) $(SERVER)
	mkdir -p ../../exec/lib
	mv $(SERVER) ../../exec/
	mv $(LIBLED) ../../exec/lib/
	mv $(LIBBUZZER) ../../exec/lib/
	mv $(LIBLIGHT) ../../exec/lib/
	mv $(LIBFND) ../../exec/lib/

clean:
	rm -f $(SERVER) $(LIBS)

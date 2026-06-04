CC = gcc
CFLAGS = -Wall -g -Icode/server/include
LDFLAGS = -ldl -lpthread
LIB_LDFLAGS = -lwiringPi -lpthread

EXEC_DIR = exec
EXEC_LIB_DIR = $(EXEC_DIR)/lib
SERVER = $(EXEC_DIR)/server
CLIENT = $(EXEC_DIR)/client
WEB = $(EXEC_DIR)/web_status
LIBLED = $(EXEC_LIB_DIR)/libled.so
LIBBUZZER = $(EXEC_LIB_DIR)/libbuzzer.so
LIBLIGHT = $(EXEC_LIB_DIR)/liblight.so
LIBFND = $(EXEC_LIB_DIR)/libfnd.so
LIBS = $(LIBLED) $(LIBBUZZER) $(LIBLIGHT) $(LIBFND)

SERVER_SRCS = code/server/src/server.c code/server/src/daemon.c
CLIENT_SRCS = code/client/client.c
WEB_SRCS = code/server/src/web_status.c
RUNNING_LOG = docs/running.txt

all: server client

server: reset-log $(SERVER)

client: $(CLIENT)

web: $(WEB)

reset-log:
	mkdir -p docs
	: > $(RUNNING_LOG)

$(EXEC_DIR):
	mkdir -p $(EXEC_DIR)

$(EXEC_LIB_DIR): | $(EXEC_DIR)
	mkdir -p $(EXEC_LIB_DIR)

$(LIBLED): code/server/lib/led.c code/server/include/led.h code/server/include/device_log.h | $(EXEC_LIB_DIR)
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBLED) code/server/lib/led.c $(LIB_LDFLAGS)

$(LIBBUZZER): code/server/lib/buzzer.c code/server/include/buzzer.h code/server/include/device_log.h | $(EXEC_LIB_DIR)
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBBUZZER) code/server/lib/buzzer.c $(LIB_LDFLAGS)

$(LIBLIGHT): code/server/lib/light.c code/server/include/light.h code/server/include/led.h code/server/include/device_log.h | $(EXEC_LIB_DIR)
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBLIGHT) code/server/lib/light.c $(LIB_LDFLAGS)

$(LIBFND): code/server/lib/fnd.c code/server/include/fnd.h code/server/include/device_log.h | $(EXEC_LIB_DIR)
	$(CC) $(CFLAGS) -fPIC -shared -o $(LIBFND) code/server/lib/fnd.c $(LIB_LDFLAGS)

$(SERVER): $(SERVER_SRCS) $(LIBS) | $(EXEC_DIR)
	$(CC) $(CFLAGS) -o $(SERVER) $(SERVER_SRCS) $(LDFLAGS)

$(CLIENT): $(CLIENT_SRCS) | $(EXEC_DIR)
	$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT_SRCS)

$(WEB): $(WEB_SRCS) | $(EXEC_DIR)
	$(CC) $(CFLAGS) -o $(WEB) $(WEB_SRCS)

clean:
	rm -f $(SERVER) $(CLIENT) $(WEB) $(LIBS)
	rmdir --ignore-fail-on-non-empty $(EXEC_LIB_DIR) 2>/dev/null || true
	rmdir --ignore-fail-on-non-empty $(EXEC_DIR) 2>/dev/null || true

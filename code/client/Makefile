CC = gcc
CFLAGS = -Wall -g
TARGET = client
SRCS = client.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all server client clean

all: server

server:
	$(MAKE) -C code/server

client:
	$(MAKE) -C code/client

clean:
	$(MAKE) -C code/server clean
	$(MAKE) -C code/client clean

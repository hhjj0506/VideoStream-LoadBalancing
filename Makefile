CC = g++
CFLAGS = -std=c++11 $(shell pkg-config --cflags opencv)
LDFLAGS = $(shell pkg-config --libs opencv)

SERVER_SRC = server.cpp
CLIENT_SRC = client.cpp

SERVER_TARGET = server
CLIENT_TARGET = client

.PHONY: all server client clean

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $< -o $(SERVER_TARGET) $(LDFLAGS)

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) $< -o $(CLIENT_TARGET) $(LDFLAGS)

clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

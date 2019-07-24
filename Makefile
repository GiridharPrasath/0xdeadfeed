CC = gcc
IPATH = /usr/local/include/
LIBPATH = /usr/local/lib/
PROTOBUF_GEN = chatmessage.pb-c

CFLAGS = -I $(IPATH) -L $(LIBPATH) -g -Wall
LFLAGS = -lprotobuf-c -pthread

SERVER_TARGET = chatserver
CLIENT_TARGET = chatclient

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_TARGET).c
	$(CC) $(CFLAGS) $(PROTOBUF_GEN).c $(SERVER_TARGET).c -o $(SERVER_TARGET) $(LFLAGS)

$(CLIENT_TARGET): $(CLIENT_TARGET).c
	$(CC) $(CFLAGS) $(PROTOBUF_GEN).c $(CLIENT_TARGET).c -o $(CLIENT_TARGET) $(LFLAGS)

clean:
	rm $(SERVER_TARGET)
	rm $(CLIENT_TARGET)

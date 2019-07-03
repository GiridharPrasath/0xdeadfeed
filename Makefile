CC = g++
IPATH = /usr/local/include/
LIBPATH = /usr/local/lib/
PROTOBUF_GEN = chatmessage.pb

CFLAGS = -I $(IPATH) -L $(LIBPATH) -g -Wall
LFLAGS = -lprotobuf -pthread

SERVER_TARGET = chatserver
CLIENT_TARGET = chatclient

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_TARGET).cpp
	$(CC) $(CFLAGS) $(SERVER_TARGET).cpp $(PROTOBUF_GEN).cc -o $(SERVER_TARGET) $(LFLAGS)

$(CLIENT_TARGET): $(CLIENT_TARGET).cpp
	$(CC) $(CFLAGS) $(CLIENT_TARGET).cpp $(PROTOBUF_GEN).cc -o $(CLIENT_TARGET) $(LFLAGS)


clean:
	rm $(SERVER_TARGET)
	rm $(CLIENT_TARGET)

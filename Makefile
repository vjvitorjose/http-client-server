CC = gcc
CFLAGS = -g -Wall -Wextra -std=c11
LDFLAGS =

SERVER_DIR = server
CLIENT_DIR = client

SERVER_SRC = $(SERVER_DIR)/server.c
SERVER_EXE = $(SERVER_DIR)/meu_servidor

CLIENT_SRC = $(CLIENT_DIR)/client.c
CLIENT_EXE = $(CLIENT_DIR)/meu_navegador

.PHONY: all
all: $(SERVER_EXE) $(CLIENT_EXE)

$(SERVER_EXE): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(CLIENT_EXE): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(SERVER_EXE) $(CLIENT_EXE)
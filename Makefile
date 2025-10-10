# Compiler
CC = gcc
CFLAGS = -Wall -Wextra -g

# Directories
CLIENT_DIR = client
SERVER_DIR = server
BUILD_DIR = build

# Sources
CLIENT_SRCS = $(wildcard $(CLIENT_DIR)/*.c)
SERVER_SRCS = $(wildcard $(SERVER_DIR)/*.c)

# Output binaries
CLIENT_BIN = $(BUILD_DIR)/client
SERVER_BIN = $(BUILD_DIR)/server

# Libraries
CLIENT_LIBS = -lreadline -lpthread
SERVER_LIBS = 

# Default target
all: $(BUILD_DIR) $(CLIENT_BIN) $(SERVER_BIN)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Client build
$(CLIENT_BIN): $(CLIENT_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CLIENT_SRCS) -o $@ $(CLIENT_LIBS)

# Server build
$(SERVER_BIN): $(SERVER_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SERVER_SRCS) -o $@ $(SERVER_LIBS)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

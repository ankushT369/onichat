# Compiler
CC = gcc

# Directories
CLIENT_DIR = client
SERVER_DIR = server
COMMON_DIR = common
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

# Build type (Debug or Release)
BUILD ?= Debug

ifeq ($(BUILD),Debug)
    CFLAGS = -Wall -Wextra -g -I$(COMMON_DIR) -I$(SERVER_DIR) -I$(CLIENT_DIR)
else ifeq ($(BUILD),Release)
    CFLAGS = -O2 -I$(COMMON_DIR) -I$(SERVER_DIR) -I$(CLIENT_DIR)
endif

# Source files
CLIENT_SRCS = $(wildcard $(CLIENT_DIR)/*.c)
SERVER_SRCS = $(wildcard $(SERVER_DIR)/*.c)
COMMON_SRCS = $(wildcard $(COMMON_DIR)/*.c)

# Separate client/server common sources
CLIENT_COMMON_SRCS = $(filter-out $(COMMON_DIR)/log.c, $(COMMON_SRCS))
SERVER_COMMON_SRCS = $(COMMON_SRCS)

# Object files (mirror source paths in build/)
CLIENT_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))
SERVER_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))
CLIENT_COMMON_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CLIENT_COMMON_SRCS))
SERVER_COMMON_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SERVER_COMMON_SRCS))

# Output binaries
CLIENT_BIN = $(BIN_DIR)/onichat
SERVER_BIN = $(BIN_DIR)/onichat-server

# Libraries
STATIC ?= 0
ifeq ($(STATIC),1)
    CLIENT_LIBS = /usr/lib/x86_64-linux-gnu/libreadline.a /usr/lib/x86_64-linux-gnu/libtinfo.a -DCLIENT -lpthread -static
    SERVER_LIBS = -lzlog -lpthread -static
else
    CLIENT_LIBS = -lreadline -lncurses -lpthread -DCLIENT
    SERVER_LIBS = -lzlog -lpthread
endif

# Default target
all: client server

# Ensure bin directory exists
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Pattern rule: compile .c -> .o, create directories automatically
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Client build: link objects
$(CLIENT_BIN): $(CLIENT_OBJS) $(CLIENT_COMMON_OBJS)
	@echo "Linking onichat (client)..."
	$(CC) $(CLIENT_OBJS) $(CLIENT_COMMON_OBJS) -o $@ $(CLIENT_LIBS)

# Server build: link objects
$(SERVER_BIN): $(SERVER_OBJS) $(SERVER_COMMON_OBJS)
	@echo "Linking onichat-server..."
	$(CC) $(SERVER_OBJS) $(SERVER_COMMON_OBJS) -o $@ $(SERVER_LIBS)

# Separate targets for independent builds
client: $(BIN_DIR) $(CLIENT_BIN)
server: $(BIN_DIR) $(SERVER_BIN)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean client server

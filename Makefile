# Compiler
CC = gcc

# Platform detection
UNAME_S := $(shell uname -s)
ANDROID := 0

# Detect Android (Termux)
ifneq ($(wildcard /data/data/com.termux/files/usr),)
    ANDROID = 1
endif

# Directories
CLIENT_DIR = client
SERVER_DIR = server
COMMON_DIR = common
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

# Build type (Debug or Release)
BUILD ?= Debug

# Base CFLAGS
CFLAGS = -I$(COMMON_DIR) -I$(SERVER_DIR) -I$(CLIENT_DIR)

ifeq ($(BUILD),Debug)
    CFLAGS += -Wall -Wextra -g
else ifeq ($(BUILD),Release)
    CFLAGS += -O2
endif

# Android-specific adjustments
ifeq ($(ANDROID),1)
    # Android/Termux doesn't have libncurses but uses libreadline with built-in termcap
    CLIENT_LIBS_ANDROID = -lreadline -lzlog -lpthread
    SERVER_LIBS_ANDROID = -lzlog
    # You might need to adjust include paths for Android if needed
    # CFLAGS += -I/data/data/com.termux/files/usr/include
else
    # Standard Linux libraries
    CLIENT_LIBS_STANDARD = -lreadline -lncurses -lpthread -lzlog
    SERVER_LIBS_STANDARD = -lzlog
endif

# Source files
CLIENT_SRCS = $(wildcard $(CLIENT_DIR)/*.c)
SERVER_SRCS = $(wildcard $(SERVER_DIR)/*.c)
COMMON_SRCS = $(wildcard $(COMMON_DIR)/*.c)

# Object files (mirror source paths in build/)
CLIENT_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))
SERVER_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))
COMMON_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(COMMON_SRCS))

# Output binaries
CLIENT_BIN = $(BIN_DIR)/onichat
SERVER_BIN = $(BIN_DIR)/onichat-server

# Libraries selection based on platform
STATIC ?= 0
ifeq ($(STATIC),1)
    ifeq ($(ANDROID),1)
        $(error Static linking not supported on Android)
    else
        CLIENT_LIBS = /usr/lib/x86_64-linux-gnu/libreadline.a /usr/lib/x86_64-linux-gnu/libtinfo.a -lzlog -lpthread -static
        SERVER_LIBS = -lzlog -lpthread -static
    endif
else
    ifeq ($(ANDROID),1)
        CLIENT_LIBS = $(CLIENT_LIBS_ANDROID)
        SERVER_LIBS = $(SERVER_LIBS_ANDROID)
    else
        CLIENT_LIBS = $(CLIENT_LIBS_STANDARD)
        SERVER_LIBS = $(SERVER_LIBS_STANDARD)
    endif
endif

# Default target
all: client server

# Android-specific target (client only, no server if zlog is problematic)
android: client-android-nozlog

# Ensure bin directory exists
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Pattern rule: compile .c -> .o, create directories automatically
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Client build: link objects
$(CLIENT_BIN): $(CLIENT_OBJS) $(COMMON_OBJS)
	@echo "Linking onichat..."
	$(CC) $(CLIENT_OBJS) $(COMMON_OBJS) -o $@ $(CLIENT_LIBS)

# Server build: link objects
$(SERVER_BIN): $(SERVER_OBJS) $(COMMON_OBJS)
	@echo "Linking onichat-server..."
	$(CC) $(SERVER_OBJS) $(COMMON_OBJS) -o $@ $(SERVER_LIBS)

# Android-specific client build (manual compilation like you did)
#client-android:
#	@echo "Building for Android (Termux)..."
#	@mkdir -p $(BIN_DIR)
#	$(CC) $(CFLAGS) -o $(CLIENT_BIN) \
#		$(COMMON_DIR)/ini.c $(COMMON_DIR)/protocol.c $(COMMON_DIR)/log.c \
#		$(CLIENT_DIR)/color.c $(CLIENT_DIR)/tclient.c $(CLIENT_DIR)/tconnect.c \
#		-lreadline -lzlog -lpthread
#		@echo "Android build complete: $(CLIENT_BIN)"

# Alternative: Android build without zlog
client-android:
	@echo "Building for Android without zlog..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -DNO_ZLOG -o $(CLIENT_BIN) \
		$(COMMON_DIR)/ini.c $(COMMON_DIR)/protocol.c $(COMMON_DIR)/log.c \
		$(CLIENT_DIR)/color.c $(CLIENT_DIR)/tclient.c $(CLIENT_DIR)/tconnect.c \
		-lreadline -lpthread
	@echo "Android build without zlog complete: $(CLIENT_BIN)"

# Separate targets for independent builds
client: $(BIN_DIR) $(CLIENT_BIN)

server: $(BIN_DIR) $(SERVER_BIN)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Info target to show build configuration
info:
	@echo "Platform: $(UNAME_S)"
	@echo "Android: $(ANDROID)"
	@echo "Build type: $(BUILD)"
	@echo "Static: $(STATIC)"
	@echo "Client libs: $(CLIENT_LIBS)"
	@echo "Server libs: $(SERVER_LIBS)"

.PHONY: all clean client server android client-android info

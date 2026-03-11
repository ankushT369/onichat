APP := onichat
SRC := onichat.go
BUILD := build

OS := $(shell go env GOOS)
ARCH := $(shell go env GOARCH)

BIN := $(APP)-$(OS)-$(ARCH)

all: clean build

build:
	@mkdir -p $(BUILD)
	CGO_ENABLED=1 go build -tags libtor -o $(BUILD)/$(BIN) $(SRC)

clean:
	rm -rf $(BUILD)

.PHONY: all build clean

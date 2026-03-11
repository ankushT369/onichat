APP := onichat
SRC := onichat.go
BUILD := build

all: clean build

build:
	@mkdir -p $(BUILD)
	go build -o $(BUILD)/$(APP) $(SRC)

clean:
	rm -rf $(BUILD)

.PHONY: all build clean run-server run-client

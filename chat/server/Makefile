# Compiler and Linker
CC := gcc
LD := gcc

# Directories
SRC_DIR := src
BUILD_DIR := build

# Flags
CFLAGS := -Wall -Wextra -I$(SRC_DIR)
LDFLAGS := -pthread
LIBS := -lhiredis

# Files
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Targets
all: server

server: $(OBJS)
	$(LD) $^ -o $@ $(LDFLAGS) $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean docker docker-run

clean:
	rm -rf $(BUILD_DIR)/* server

docker:
	docker build -t server .

docker-run:
	docker run -p 5000:5000 -it server

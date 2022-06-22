# Used to build practice programs for CSAPP.

BUILD_DIR := build
SRC_DIR := src
BIN_DIR := bin
# contains library headers and sources
LIB_DIR := lib
# Global used libraries
GLOBAL_LIBS := libcsapp.so libserve_api.so

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)
OUTS := $(SRCS:%.c=$(BUILD_DIR)/%)

CPPFLAGS := $(INC_FLAGS) -Wall -g -Iinclude

all: $(OUTS)
	mkdir -p $(BIN_DIR)
	cp $^ $(BIN_DIR)

$(BUILD_DIR)/%: $(BUILD_DIR)/%.o $(GLOBAL_LIBS:%=$(BUILD_DIR)/%)
	$(CC) $^ -o $@ $(LDFLAGS) -ljson-c

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# libcsapp.so
$(BUILD_DIR)/libcsapp.so: $(BUILD_DIR)/csapp.o
	$(CC) -shared -o $@ $^

$(BUILD_DIR)/csapp.o: $(LIB_DIR)/csapp.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -fpic -c -o $@ $^

# libserve_api.so
$(BUILD_DIR)/libserve_api.so: $(BUILD_DIR)/serve_api.o
	$(CC) -shared -o $@ $^

$(BUILD_DIR)/serve_api.o: $(LIB_DIR)/serve_api.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -fpic -c -o $@ $^

.PHONY: clean
clean:
	-rm -r $(BUILD_DIR) $(BIN_DIR)

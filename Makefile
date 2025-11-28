CXX := g++
CC  := gcc

CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Iinclude -Ivendor/kissfft
CFLAGS   := -O2 -Wall -Wextra -Iinclude -Ivendor/kissfft

SRC_DIR := src
KISS_DIR := vendor/kissfft
BUILD_DIR := build
BIN_DIR := bin

SRCS := $(shell find $(SRC_DIR) -name '*.cpp')
CSRCS := $(shell find $(KISS_DIR)/*.c)

OBJS :=  $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
COBJS := $(CSRCS:$(KISS_DIR)/%.c=$(BUILD_DIR)/%.o)

TARGET := $(BIN_DIR)/sampler

LIBS := \
    -lglfw -lGLEW -lGL \
    -lportaudio -lasound -lm -lpthread \
    -lsndfile

all: $(TARGET)

$(TARGET): $(OBJS) $(COBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)
	
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	
$(BUILD_DIR)/%.o: $(KISS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run: all
	@DEV=$$(lsusb | grep "0763:103b" | awk '{print $$2 "/" $$4}' | sed 's/://'); \
	if [ -z "$$DEV" ]; then \
		echo "ERROR: Keyboard not found (ID 0763:103b)"; \
		exit 1; \
	fi; \
	DEV_PATH="/dev/bus/usb/$$DEV"; \
	$(TARGET) "$$DEV_PATH"

.PHONY: all clean run

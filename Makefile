CC := gcc
CFLAGS := -g -std=gnu17
CPPFLAGS := $(shell sdl2-config --cflags) -MP -MMD
LDFLAGS := $(shell sdl2-config --libs)

BUILD_DIR := ./build
SRC_DIR := ./src

TARGET_EXEC := gbemu

SRCS := $(basename $(notdir $(wildcard $(SRC_DIR)/*.c)))
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all
all: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $^ $(LDFLAGS)
	cp $(BUILD_DIR)/$(TARGET_EXEC) ./$(TARGET_EXEC)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)%    
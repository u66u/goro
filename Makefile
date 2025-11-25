CC = gcc
CFLAGS = -O3 -g -Wall -Wextra -pthread -Isrc

SRC_DIR = src
BUILD_DIR = build

LIB_SRCS = $(wildcard $(SRC_DIR)/*.c)
MAIN_SRC = main.c

LIB_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(LIB_SRCS))
MAIN_OBJ = $(BUILD_DIR)/main.o

TARGET = coro

all: $(TARGET)

$(TARGET): $(LIB_OBJS) $(MAIN_OBJ)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) -o $@ $^


$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@


$(BUILD_DIR)/main.o: $(MAIN_SRC)
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	@echo "Cleaning..."
	rm -rf $(BUILD_DIR) $(TARGET)


run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
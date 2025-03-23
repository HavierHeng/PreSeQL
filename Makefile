# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = tests

# Create directory structure if it doesn't exist
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# Source files and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Main target
TARGET = $(BIN_DIR)/program

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -I$(INC_DIR)

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INC_DIR)

# Clean build files
clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*

# Build and run
run: all
	./$(TARGET)

# Build tests
test: all
	$(CC) $(CFLAGS) $(TEST_DIR)/*.c -o $(BIN_DIR)/tests -I$(INC_DIR)
	./$(BIN_DIR)/tests

# Include header file dependencies
-include $(OBJS:.o=.d)

# Phony targets
.PHONY: all clean run test

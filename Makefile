
# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./src

# Directories
SRC_DIR = src
OBJ_DIR = build/obj
BIN_DIR = build/bin
TEST_DIR = test

# Source files by category (excluding main files)
COMPILER_SRCS = $(wildcard $(SRC_DIR)/compiler/*.c)
ALGORITHM_SRCS = $(wildcard $(SRC_DIR)/algorithm/*.c)
# PAGER_SRCS = $(wildcard $(SRC_DIR)/pager/*.c) \
#              $(wildcard $(SRC_DIR)/pager/db/data/*.c) \
#              $(wildcard $(SRC_DIR)/pager/db/index/*.c) \
#              $(wildcard $(SRC_DIR)/pager/db/overflow/*.c) \
#              $(wildcard $(SRC_DIR)/pager/journal/journal_data/*.c)
# VM_SRCS = $(wildcard $(SRC_DIR)/vm_engine/*.c)
ALLOCATOR_SRCS = $(wildcard $(SRC_DIR)/allocator/*.c)
STATUS_SRCS = $(wildcard $(SRC_DIR)/status/*.c)
TYPES_SRCS = $(wildcard $(SRC_DIR)/types/*.c)

# All source files (excluding main.c)
SRCS = $(COMPILER_SRCS) $(ALGORITHM_SRCS) $(PAGER_SRCS) $(VM_SRCS) \
       $(ALLOCATOR_SRCS) $(STATUS_SRCS) $(TYPES_SRCS)

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Create build directories
$(shell mkdir -p $(BIN_DIR) \
	$(OBJ_DIR)/compiler \
	$(OBJ_DIR)/algorithm \
	$(OBJ_DIR)/pager/db/data \
	$(OBJ_DIR)/pager/db/index \
	$(OBJ_DIR)/pager/db/overflow \
	$(OBJ_DIR)/pager/journal/journal_data \
	$(OBJ_DIR)/vm_engine \
	$(OBJ_DIR)/allocator \
	$(OBJ_DIR)/status \
	$(OBJ_DIR)/types)

# Default target
all: preseql

# Main CLI program
preseql: $(OBJS) $(OBJ_DIR)/main.o
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

# Test radix tree
test_radix: $(OBJ_DIR)/algorithm/radix_tree.o $(OBJ_DIR)/tests/test_radix.o
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

# Test pager subsystem
test_pager: $(OBJ_DIR)/pager/pager.o $(OBJ_DIR)/algorithm/radix_tree.o $(OBJ_DIR)/algorithm/crc.o \
           $(OBJ_DIR)/pager/db/index/btree.o \
           $(OBJ_DIR)/pager/db/data/data_page.o $(OBJ_DIR)/pager/db/overflow/overflow_page.o \
           $(OBJ_DIR)/pager/db/free_space.o $(OBJ_DIR)/tests/test_pager.o
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^


# Compile main.c
$(OBJ_DIR)/main.o: $(SRC_DIR)/client.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test_radix.c
$(OBJ_DIR)/tests/test_radix.o: $(TEST_DIR)/test_radix.c
	@mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test_pager.c
$(OBJ_DIR)/tests/test_pager.o: $(TEST_DIR)/test_pager.c
	@mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CFLAGS) -c $< -o $@

# Generic rule for compiling source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf build/bin/ build/obj/

# Run the main CLI program
run: preseql
	$(BIN_DIR)/preseql

# Run the radix tree test
run_radix: test_radix
	$(BIN_DIR)/test_radix

# Run the pager test
run_pager: test_pager
	$(BIN_DIR)/test_pager

# Phony targets
.PHONY: all clean run run_radix run_pager preseql test_radix test_pager

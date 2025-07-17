CC = C:/msys64/mingw64/bin/gcc.exe
CXX = C:/msys64/mingw64/bin/g++.exe
CFLAGS = -Iinclude -I/mingw64/include -Wall -Wextra -std=c11 -g
CXXFLAGS = -Iinclude -I/mingw64/include -Wall -Wextra -std=c++11 -g -Wno-shadow -Wno-unused-parameter

BIN_DIR := bin
SRC_DIR := source
TOOLS_DIR := tools
INCLUDE_DIR := include

# Libraries
LIBS = -lm4ri -lm

# Core library files
CORE_SOURCES = $(SRC_DIR)/crypto_lib.c $(SRC_DIR)/encrypt.c
CORE_HEADERS = $(INCLUDE_DIR)/crypto_lib.h $(INCLUDE_DIR)/encrypt.h

# Tool files
TOOL_SOURCES = $(TOOLS_DIR)/gen_zS_bin.c $(TOOLS_DIR)/gen_s_gt_bin_no_header.c

all: $(BIN_DIR) core_library source_executables

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Core library (object files)
core_library: $(SRC_DIR)/crypto_lib.o

$(SRC_DIR)/crypto_lib.o: $(SRC_DIR)/crypto_lib.c $(CORE_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

# Tools
# Only build tools that exist
tools: gen_zS_bin gen_s_gt_bin_no_header

gen_zS_bin: $(TOOLS_DIR)/gen_zS_bin.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/gen_zS_bin.exe $< $(LIBS)

gen_s_gt_bin_no_header: $(TOOLS_DIR)/gen_s_gt_bin_no_header.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/gen_s_gt_bin_no_header.exe $< $(LIBS)

# Utility targets
debug_init: $(SRC_DIR)/debug_init.c $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/debug_init.exe $^ $(LIBS)

# Source executables
source_executables: debug_init simple_test

simple_test: $(SRC_DIR)/simple_test.c $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/simple_test.exe $^ $(LIBS)

# Clean target
clean:
ifeq ($(OS),Windows_NT)
	@echo "Cleaning (Windows)..."
	@del /Q $(BIN_DIR)\*.exe 2>NUL || echo "No executables to delete"
	@del /Q $(SRC_DIR)\*.o 2>NUL || echo "No object files to delete"
else
	@echo "Cleaning (Unix)..."
	@rm -f $(BIN_DIR)/*.exe
	@rm -f $(SRC_DIR)/*.o
endif
.PHONY: all clean core_library tools debug_init simple_test 
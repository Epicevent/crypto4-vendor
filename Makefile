# ── Compiler settings ───────────────────────────────────────────────────
CC        := gcc
CXX       := g++
BIN_DIR   := bin
TEST_TIMER:= ct_build_test
SRC_DIR   := source
TOOLS_DIR := tools
INCLUDE   := include
TEST_DIR  := test
M4RI_DIR  := 3rdparty/m4ri
M4RI_BUILD:= $(M4RI_DIR)/build
M4RI_LIB  := $(M4RI_BUILD)/lib/libm4ri.a

# ── Flags ───────────────────────────────────────────────────────────────
CFLAGS      := -I$(INCLUDE) -I$(M4RI_BUILD)/include -Wall -Wextra -std=c11 -g -w
LDFLAGS     := -L$(M4RI_BUILD)/lib -lm4ri -lm

CXXFLAGS    := -I$(INCLUDE) -I$(M4RI_BUILD)/include -Wall -Wextra -std=c++11 -g
LDFLAGS_CXX := $(LDFLAGS)

.PHONY: all m4ri core tools clean

all: core tools  $(TEST_TIMER)

# ── 1) Build & install M4RI submodule ────────────────────────────────────
m4ri: $(M4RI_LIB)

$(M4RI_LIB):
	@echo "==> Building M4RI submodule..."
	@mkdir -p $(M4RI_BUILD)
	@cd $(M4RI_DIR) && autoreconf -fi
	@cd $(M4RI_BUILD) && ../configure --disable-shared --prefix=$$(pwd) && make && make install

# ── 2) Core executables ─────────────────────────────────────────────────
core: debug_init simple_test decrypt_test

debug_init: $(TEST_DIR)/debug_init.c $(SRC_DIR)/encrypt.c $(INCLUDE)/encrypt.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/debug_init $(LDFLAGS)

simple_test: $(TEST_DIR)/simple_test.c $(SRC_DIR)/encrypt.c $(INCLUDE)/encrypt.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/simple_test $(LDFLAGS)

decrypt_test: $(TEST_DIR)/decrypt_test.c $(SRC_DIR)/decrypt.c $(SRC_DIR)/encrypt.c \
              $(INCLUDE)/decrypt.h $(INCLUDE)/encrypt.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/decrypt_test $(LDFLAGS)

# ── 2.1) Timing‐harness for Ct_cache build ─────────────────────────────────
ct_build_test: $(TEST_DIR)/ct_build_test.c $(SRC_DIR)/encrypt.c $(SRC_DIR)/decrypt.c \
	$(INCLUDE)/encrypt.h $(INCLUDE)/decrypt.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/ct_build_test $(LDFLAGS)
	@echo "Built Ct‐cache timing test"

# ── 3) Tools ────────────────────────────────────────────────────────────
tools: gen_zS_bin gen_s_gt_bin gen_r4_patterns verify_r4_pattern_rule gen_H_bin

gen_zS_bin:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TOOLS_DIR)/gen_zS_bin.c -o $(BIN_DIR)/gen_zS_bin $(LDFLAGS)

gen_s_gt_bin:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TOOLS_DIR)/gen_s_gt_bin.c -o $(BIN_DIR)/gen_s_gt_bin $(LDFLAGS)

gen_r4_patterns:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TOOLS_DIR)/gen_r4_patterns.c -o $(BIN_DIR)/gen_r4_patterns $(LDFLAGS)

verify_r4_pattern_rule:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TOOLS_DIR)/verify_r4_pattern_rule.c -o $(BIN_DIR)/verify_r4_pattern_rule $(LDFLAGS)

gen_H_bin:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TOOLS_DIR)/gen_H_bin.c $(SRC_DIR)/encrypt.c -o $(BIN_DIR)/gen_H_bin $(LDFLAGS)

# ── 4) Clean ─────────────────────────────────────────────────────────────
clean:
	@echo "==> Cleaning..."
	@rm -rf $(BIN_DIR)/*

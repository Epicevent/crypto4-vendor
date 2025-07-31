# ── Compiler settings ───────────────────────────────────────────────────
CC        := gcc
AR        := ar rcs
BIN_DIR   := bin
LIB_DIR   := lib
SRC_DIR   := source
TEST_DIR  := test
TOOLS_DIR := tools
INCLUDE   := include

M4RI_DIR   := 3rdparty/m4ri
M4RI_BUILD := $(M4RI_DIR)/build
M4RI_LIB   := $(M4RI_BUILD)/lib/libm4ri.a

CFLAGS     := -I$(INCLUDE) -I$(M4RI_BUILD)/include -Wall -Wextra -std=c11 -g -w
LDFLAGS    := -L$(M4RI_BUILD)/lib -lm4ri -lm

.PHONY: all m4ri libcrypto encrypt_tool simple_test decrypt_tool decrypt_test ct_build_test tools clean

all: m4ri libcrypto encrypt_tool simple_test decrypt_tool decrypt_test ct_build_test tools

# ── 1) Build & install M4RI submodule ────────────────────────────────────
m4ri: $(M4RI_LIB)

$(M4RI_LIB):
	@echo "==> Building M4RI submodule..."
	@mkdir -p $(M4RI_BUILD)
	@cd $(M4RI_DIR) && autoreconf -fi
	@cd $(M4RI_BUILD) && ../configure --disable-shared --prefix=$$(pwd) && make && make install

# ── 2) Core library (encrypt, decrypt, lfsr_state) ───────────────────────
libcrypto: $(LIB_DIR)/libcrypto.a

$(LIB_DIR)/libcrypto.a: \
    $(SRC_DIR)/lfsr_state.c \
    $(SRC_DIR)/encrypt.c \
    $(SRC_DIR)/decrypt.c \
	$(SRC_DIR)/error_bits.c \

	@mkdir -p $(LIB_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/lfsr_state.c -o lfsr_state.o
	# decrypt.o를 만들 때 헤더 의존성 추가
	$(CC) $(CFLAGS) -c $(SRC_DIR)/decrypt.c \
	    -o decrypt.o \
	    -I$(INCLUDE)  \
	    -I$(M4RI_BUILD)/include


	# encrypt.o에도 동일하게…
	$(CC) $(CFLAGS) -c $(SRC_DIR)/encrypt.c -o encrypt.o

	# error_bits.o에도 동일하게…
	$(CC) $(CFLAGS) -c $(SRC_DIR)/error_bits.c -o error_bits.o

	$(AR) $@ lfsr_state.o decrypt.o encrypt.o error_bits.o
	@rm -f lfsr_state.o decrypt.o encrypt.o error_bits.o
# ── 3) Application targets ───────────────────────────────────────────────

decrypt_tool: libcrypto
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/decrypt_tool.c \
	    -L$(LIB_DIR) -lcrypto $(LDFLAGS) -o $(BIN_DIR)/decrypt_tool
	@echo "Built decrypt_tool"
## simple_test: simple_test.c 에서 main()을 제공
simple_test: libcrypto
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/simple_test.c \
	    -L$(LIB_DIR) -lcrypto $(LDFLAGS) -o $(BIN_DIR)/simple_test
	@echo "Built simple_test"


find_r4: libcrypto
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/find_r4.c \
	    -L$(LIB_DIR) -lcrypto $(LDFLAGS) -o $(BIN_DIR)/find_r4
	@echo "Built find_r4"

encrypt_test: libcrypto
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/encrypt_test.c \
	    -L$(LIB_DIR) -lcrypto $(LDFLAGS) -o $(BIN_DIR)/encrypt_test


## decrypt_test: decrypt_test.c 에서 main()을 제공
decrypt_test: libcrypto
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/decrypt_test.c \
	    -L$(LIB_DIR) -lcrypto $(LDFLAGS) -o $(BIN_DIR)/decrypt_test

## ct_build_test: ct_build_test.c 에서 main()을 제공
ct_build_test: libcrypto
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/ct_build_test.c \
	    -L$(LIB_DIR) -lcrypto $(LDFLAGS) -o $(BIN_DIR)/ct_build_test
	@echo "Built Ct‑cache timing test"

## test_error_config: error_bits.c 에서 main()을 제공
test_error_config: libcrypto
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/test_error_config.c \
	    -L$(LIB_DIR) -lcrypto $(LDFLAGS) -o $(BIN_DIR)/test_error_config
	@echo "Built test_error_config"

# ── 4) Tools ────────────────────────────────────────────────────────────
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
	$(CC) $(CFLAGS) $(TOOLS_DIR)/gen_H_bin.c  -o $(BIN_DIR)/gen_H_bin $(LDFLAGS)

# ── 5) Clean ─────────────────────────────────────────────────────────────
clean:
	@echo "==> Cleaning..."
	@rm -rf $(BIN_DIR) $(LIB_DIR)/*.a

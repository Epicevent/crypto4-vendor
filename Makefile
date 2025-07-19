# Makefile

# ── 컴파일러 설정 ───────────────────────────────────────────────────
CC      := gcc
CXX     := g++

# ── 디렉터리 변수 ───────────────────────────────────────────────────
BIN_DIR     := bin
SRC_DIR     := source
TOOLS_DIR   := tools
INCLUDE_DIR := include
M4RI_DIR    := 3rdparty/m4ri
M4RI_BUILD  := $(M4RI_DIR)/build
M4RI_LIB    := $(M4RI_BUILD)/lib/libm4ri.a

# ── 컴파일 플래그 및 링커 옵션 ───────────────────────────────────────
CFLAGS  := \
  -I$(INCLUDE_DIR) \
  -I$(M4RI_BUILD)/include \
  -Wall -Wextra -std=c11 -g

LDFLAGS := -L$(M4RI_BUILD)/lib
LIBS    := -lm4ri -lm

CXXFLAGS := \
  -I$(INCLUDE_DIR) \
  -I$(M4RI_BUILD)/include \
  -Wall -Wextra -std=c++11 -g \
  -Wno-shadow -Wno-unused-parameter

# ── 기본 타겟 ───────────────────────────────────────────────────────
.PHONY: all m4ri core tools clean
all: core 

# ── 1) M4RI 서브모듈 빌드 & 설치 ────────────────────────────────────
m4ri: $(M4RI_LIB)

$(M4RI_LIB):
	@echo "==> Building M4RI submodule..."
	@mkdir -p $(M4RI_BUILD)
	@cd $(M4RI_DIR)   && autoreconf -fi
	@cd $(M4RI_BUILD) && \
		../configure --disable-shared --prefix=$$(pwd) && \
		make && make install

# ── 2) Core 실행파일 빌드 ───────────────────────────────────────────
core: $(SRC_DIR)/crypto_lib.o $(SRC_DIR)/encrypt.o
	@echo "==> Linking core executables..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	  $(SRC_DIR)/debug_init.c $(SRC_DIR)/crypto_lib.c $(SRC_DIR)/encrypt.c \
	  -o $(BIN_DIR)/debug_init $(LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	  $(SRC_DIR)/simple_test.c  $(SRC_DIR)/crypto_lib.c $(SRC_DIR)/encrypt.c \
	  -o $(BIN_DIR)/simple_test  $(LIBS)

$(SRC_DIR)/crypto_lib.o: $(SRC_DIR)/crypto_lib.c $(INCLUDE_DIR)/crypto_lib.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/encrypt.o: $(SRC_DIR)/encrypt.c $(INCLUDE_DIR)/encrypt.h
	$(CC) $(CFLAGS) -c $< -o $@

# ── 3) Tools 빌드 ───────────────────────────────────────────────────
tools:
	@echo "==> Building tools..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	  $(TOOLS_DIR)/gen_zS_bin.c            -o $(BIN_DIR)/gen_zS_bin            $(LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	  $(TOOLS_DIR)/gen_s_gt_bin_no_header.c -o $(BIN_DIR)/gen_s_gt_bin_no_header $(LIBS)

# ── 4) Clean ─────────────────────────────────────────────────────────
clean:
	@echo "==> Cleaning all artifacts..."
	@rm -rf $(BIN_DIR)/* $(SRC_DIR)/*.o 

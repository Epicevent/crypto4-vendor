# Makefile

# ── 컴파일러 설정  ────────────────────
CC      ?= gcc
CXX     ?= g++
  
# ── 공통 Include 경로 ───────────────────────────────────────────────────
#   - 프로젝트 헤더(include/)
#   - 3rdparty M4RI 헤더(3rdparty/m4ri/include)
CFLAGS  := -Iinclude -I3rdparty/m4ri/include -Wall -Wextra -std=c11 -g
CXXFLAGS:= -Iinclude -I3rdparty/m4ri/include -Wall -Wextra -std=c++11 -g \
            -Wno-shadow -Wno-unused-parameter

# ── 링커 옵션 ────────────────────────────────────────────────────────────
LIBS    := -lm4ri -lm

# ── 디렉터리 변수 ───────────────────────────────────────────────────────
BIN_DIR    := bin
SRC_DIR    := source
TOOLS_DIR  := tools
INCLUDE_DIR:= include

M4RI_DIR   := 3rdparty/m4ri
M4RI_BUILD := $(M4RI_DIR)/build
M4RI_LIB   := $(M4RI_BUILD)/.libs/libm4ri.a

# ── 기본 타겟 ───────────────────────────────────────────────────────────
.PHONY: all m4ri core tools clean
all: core tools

# ── (선택) M4RI 만 다시 빌드할 때 ────────────────────────────────────────
m4ri: $(M4RI_LIB)

$(M4RI_LIB):
	@echo "==> Building M4RI..."
	@mkdir -p $(M4RI_BUILD)
	@cd $(M4RI_DIR)   && autoreconf -fi
	@cd $(M4RI_BUILD) && \
		../configure --host=x86_64-w64-mingw32 --prefix=$$(pwd) --disable-shared && \
		make && make install

# ── Core 라이브러리 및 실행파일 빌드 ────────────────────────────────────
core: $(SRC_DIR)/crypto_lib.o $(SRC_DIR)/encrypt.o
	@echo "==> Linking core executables..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/debug_init.c   $(SRC_DIR)/crypto_lib.c $(SRC_DIR)/encrypt.c \
	  -o $(BIN_DIR)/debug_init.exe $(LIBS)
	$(CC) $(CFLAGS) $(SRC_DIR)/simple_test.c  $(SRC_DIR)/crypto_lib.c $(SRC_DIR)/encrypt.c \
	  -o $(BIN_DIR)/simple_test.exe $(LIBS)

$(SRC_DIR)/crypto_lib.o: $(SRC_DIR)/crypto_lib.c $(INCLUDE_DIR)/crypto_lib.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/encrypt.o: $(SRC_DIR)/encrypt.c $(INCLUDE_DIR)/encrypt.h
	$(CC) $(CFLAGS) -c $< -o $@

# ── Tools 빌드 ─────────────────────────────────────────────────────────
tools:
	@echo "==> Building tools..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(TOOLS_DIR)/gen_zS_bin.c            -o $(BIN_DIR)/gen_zS_bin.exe            $(LIBS)
	$(CC) $(CFLAGS) $(TOOLS_DIR)/gen_s_gt_bin_no_header.c -o $(BIN_DIR)/gen_s_gt_bin_no_header.exe $(LIBS)

# ── Clean ────────────────────────────────────────────────────────────────
clean:
ifeq ($(OS),Windows_NT)
	@del /Q $(BIN_DIR)\*.exe 2>NUL || true
	@del /Q $(SRC_DIR)\*.o    2>NUL || true
	@rmdir /S /Q "$(M4RI_BUILD)" || true
else
	@rm -rf $(BIN_DIR)/*.exe $(SRC_DIR)/*.o $(M4RI_BUILD)
endif

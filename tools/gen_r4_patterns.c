// Compile with:
//   mkdir -p bin
//   gcc -O2 tools/gen_r4_patterns.c -o bin/gen_r4_patterns

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define STATES      (1<<16)    // R4 상위 16비트 개수
#define PAT_LEN     (250 + 208) // discard(250) + 208
#define OUT_FILE    "data/r4_clock_patterns.bin"

// 32비트 정수의 parity 계산 함수
static inline int parity32(uint32_t x) {
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;
    return x & 1;
}

// majority 함수
static inline int maj(int a, int b, int c) {
    return (a & b) | (b & c) | (c & a);
}

// R4 LFSR 구조체 및 파라미터
typedef struct {
    uint32_t reg;
} R4_LFSR;
#define R4_FP 0x26200u
#define R4_FF 0x1FFFFu

// 초기화: LSB는 이미 1로 세팅된 state17
static inline void R4_init(R4_LFSR *R, uint32_t state17) {
    R->reg = state17 & R4_FF;
}

// 한 스텝 clock
static inline void R4_clock(R4_LFSR *R) {
    R->reg <<= 1;
    int t = parity32(R->reg & R4_FP);
    R->reg = (R->reg & R4_FF) ^ t;
}

// 특정 비트 읽기
static inline int R4_get(const R4_LFSR *R, int idx) {
    return (R->reg >> idx) & 1;
}

int main(void) {
    // 출력 파일 열기
    FILE *f = fopen(OUT_FILE, "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    uint8_t buf[PAT_LEN];

    // 모든 가능한 R4 상위 16비트에 대해 패턴 생성
    for (uint32_t hi = 0; hi < STATES; ++hi) {
        uint32_t init17 = (hi << 1) | 1;  // LSB 강제 1
        R4_LFSR R4;
        R4_init(&R4, init17);

        for (int i = 0; i < PAT_LEN; ++i) {
            int b1  = R4_get(&R4, 1);
            int b6  = R4_get(&R4, 6);
            int b15 = R4_get(&R4,15);
            int m   = maj(b1, b6, b15);

            uint8_t p = 0;
            if (m == b15) p |= 0x4;  // R1.clock()
            if (m == b6 ) p |= 0x2;  // R2.clock()
            if (m == b1 ) p |= 0x1;  // R3.clock()

            buf[i] = p;
            R4_clock(&R4);
        }

        fwrite(buf, 1, PAT_LEN, f);
    }

    fclose(f);
    return 0;
}

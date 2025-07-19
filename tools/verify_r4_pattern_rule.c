// Compile with:
//   mkdir -p bin
//   gcc -O2 tools/verify_r4_pattern_rule.c -o bin/verify_r4_pattern_rule

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define STATES         (1<<16)
#define PATTERN_LEN    458
#define BIN_FILENAME   "data/r4_clock_patterns.bin"

// R4 LFSR parameters
typedef struct { uint32_t reg; } R4_LFSR;
#define R4_FP   0x26200u
#define R4_FF   0x1FFFFu

static inline int maj(int a,int b,int c) {
    return (a & b) | (b & c) | (c & a);
}

// Clock function updated to match vendor implementation:
// 1) shift, 2) compute parity on shifted & FP, 3) apply feedback via XOR
void R4_init(R4_LFSR *R, uint32_t state17) {
    R->reg = state17 & R4_FF;
}

void R4_clock(R4_LFSR *R) {
    R->reg <<= 1;
    int t = __builtin_parity(R->reg & R4_FP);
    R->reg = (R->reg & R4_FF) ^ t;
}

int R4_get(const R4_LFSR *R, int idx) {
    return (R->reg >> idx) & 1;
}

int main() {
    // Load existing pattern file
    FILE *f = fopen(BIN_FILENAME, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    uint8_t *patterns = malloc((size_t)STATES * PATTERN_LEN);
    if (!patterns) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(f);
        return 1;
    }
    size_t read = fread(patterns, 1, (size_t)STATES * PATTERN_LEN, f);
    fclose(f);
    if (read != (size_t)STATES * PATTERN_LEN) {
        fprintf(stderr, "Read %zu bytes, expected %zu\n", read, (size_t)STATES * PATTERN_LEN);
        free(patterns);
        return 2;
    }

    // Verify each state against computed pattern
    for (uint32_t hi = 0; hi < STATES; ++hi) {
        uint32_t init17 = (hi << 1) | 1;
        R4_LFSR R4;
        R4_init(&R4, init17);
        uint8_t *buf = patterns + (size_t)hi * PATTERN_LEN;
        for (int i = 0; i < PATTERN_LEN; ++i) {
            int b1  = R4_get(&R4, 1);
            int b6  = R4_get(&R4, 6);
            int b15 = R4_get(&R4,15);
            int m   = maj(b1, b6, b15);
            uint8_t expected = 0;
            if (m == b15) expected |= 0x4;
            if (m == b6 ) expected |= 0x2;
            if (m == b1 ) expected |= 0x1;
            if (buf[i] != expected) {
                fprintf(stderr, "Mismatch at state %u, step %d: file=0x%02X, expected=0x%02X\n",
                        hi, i, buf[i], expected);
                free(patterns);
                return 3;
            }
            R4_clock(&R4);
        }
    }

    printf("All %u patterns verified OK!\n", STATES);
    free(patterns);
    return 0;
}

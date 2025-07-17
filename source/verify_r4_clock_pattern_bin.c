#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define NUM_STATES 65536
#define PATTERN_LEN 458
#define BIN_FILENAME "data/r4_clock_patterns.bin"

// R4 관련 상수
#define LFSR4_LEN 17
#define LFSR4_FP  0x26200
#define LFSR4_FF  0x1FFFF

// LFSR 구조체
typedef struct {
    int len;
    uint32_t reg;
    int fp;
    int ff;
} LFSR;

// 전역 또는 static
uint8_t clock_patterns[65536][458];

void load_clock_patterns(const char* filename) {
    FILE* f = fopen(filename, "rb");
    fread(clock_patterns, 1, 65536 * 458, f);
    fclose(f);
}

// keystream_generation 리팩토링
void keystream_generation_with_pattern(
    LFSR* R1, LFSR* R2, LFSR* R3,
    const uint8_t* pattern, // 458바이트
    int z[208]
) {
    const int discard = 250;
    for (int i = 0; i < discard + 208; i++) {
        uint8_t p = pattern[i];
        if (p & 0b100) lfsr_clock(R1);
        if (p & 0b010) lfsr_clock(R2);
        if (p & 0b001) lfsr_clock(R3);
        if (i >= discard)
            z[i - discard] = ...; // 기존 majority 등
    }
}

// 보조 함수
int majority(int a, int b, int c) {
    return (a & b) ^ (b & c) ^ (c & a);
}

int parity(int reg) {
    reg ^= (reg >> 16);
    reg ^= (reg >> 8);
    reg ^= (reg >> 4);
    reg ^= (reg >> 2);
    reg ^= (reg >> 1);
    return reg & 1;
}

void lfsr_clock(LFSR* lfsr) {
    lfsr->reg <<= 1;
    int t = parity(lfsr->reg & lfsr->fp);
    lfsr->reg = (lfsr->reg & lfsr->ff) ^ t;
}

int lfsr_get(const LFSR* lfsr, int idx) {
    return (lfsr->reg >> idx) & 1;
}

void generate_keystream(const int key[KEY_SIZE], const int nonce[NONCE_SIZE], int keystream[CIPHERTEXT_SIZE]) {
    // LFSR 초기화
    LFSR R1 = {19, 0, fp1, (1 << 19) - 1};
    LFSR R2 = {22, 0, fp2, (1 << 22) - 1};
    LFSR R3 = {23, 0, fp3, (1 << 23) - 1};
    LFSR R4 = {17, 0, fp4, (1 << 17) - 1};

    // 키로 LFSR 초기화 (예시: 각 LFSR에 대해 64회 시프트 및 XOR)
    for (int i = 0; i < 64; i++) {
        R1.reg = ((R1.reg << 1) | key[i]) & R1.ff;
        R2.reg = ((R2.reg << 1) | key[i]) & R2.ff;
        R3.reg = ((R3.reg << 1) | key[i]) & R3.ff;
        R4.reg = ((R4.reg << 1) | key[i]) & R4.ff;
    }

    // keystream 생성
    for (int i = 0; i < CIPHERTEXT_SIZE; i++) {
        int r4_lsb = R4.reg & 1;
        if (r4_lsb) {
            int r1_feedback = ((R1.reg >> 19) ^ (R1.reg >> 5) ^ (R1.reg >> 2) ^ (R1.reg >> 1) ^ R1.reg) & 1;
            R1.reg = ((R1.reg << 1) | r1_feedback) & R1.ff;
            int r2_feedback = ((R2.reg >> 22) ^ (R2.reg >> 1) ^ R2.reg) & 1;
            R2.reg = ((R2.reg << 1) | r2_feedback) & R2.ff;
            int r3_feedback = ((R3.reg >> 23) ^ (R3.reg >> 5) ^ R3.reg) & 1;
            R3.reg = ((R3.reg << 1) | r3_feedback) & R3.ff;
        }
        int r4_feedback = ((R4.reg >> 3) ^ R4.reg) & 1;
        R4.reg = ((R4.reg << 1) | r4_feedback) & R4.ff;
        keystream[i] = (R1.reg & 1) ^ (R2.reg & 1) ^ (R3.reg & 1);
    }
}

int main() {
    // my_encrypt가 clock 패턴을 메모리로 올려야 하므로, 테스트 시작 전에 한 번만 호출
    load_clock_patterns("bin/r4_clock_patterns.bin");
    compare_encrypt_functions();
    return 0;
} 
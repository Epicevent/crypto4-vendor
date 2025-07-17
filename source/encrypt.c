#include "encrypt.h"
#include <m4ri/m4ri.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crypto_lib.h"

// m4ri 기반 LFSR 상태 구조체 등은 encrypt.h에 정의되어 있다고 가정

// --- 전역 LFSR companion matrix 캐시 ---
mzd_t* A1 = NULL; // R1 companion matrix (19x19)
mzd_t* A2 = NULL; // R2 companion matrix (22x22)
mzd_t* A3 = NULL; // R3 companion matrix (23x23)
mzd_t* A4 = NULL; // R4 companion matrix (17x17)

// --- 전역 zS matrix 캐시 ---
mzd_t* zS_R1 = NULL; // zS R1 matrix (ZS_ROWS x 19)
mzd_t* zS_R2 = NULL; // zS R2 matrix (ZS_ROWS x 22)
mzd_t* zS_R3 = NULL; // zS R3 matrix (ZS_ROWS x 23)
mzd_t* zS_R4 = NULL; // zS R4 matrix (ZS_ROWS x 17)

// --- 키스케줄, 비트 리버설 등 my_encrypt.c의 논리를 m4ri로 ---

// 키스케줄: key_vec(1x64), nonce_vec(1x19) -> a_vec(1x64)
void key_scheduling_m4ri(const mzd_t* key_vec, const mzd_t* nonce_vec, mzd_t* a_vec) {
    // a[i] = K[i]
    for (int i = 0; i < 64; i++) {
        int bit = mzd_read_bit(key_vec, 0, i);
        mzd_write_bit(a_vec, 0, i, bit);
    }
    // for (int i = 3; i <= 15; i++) a[i] = a[i] ^ N[i + 3];
    for (int i = 3; i <= 15; i++) {
        int a_bit = mzd_read_bit(a_vec, 0, i);
        int n_bit = mzd_read_bit(nonce_vec, 0, i + 3);
        mzd_write_bit(a_vec, 0, i, a_bit ^ n_bit);
    }
    // for (int i = 22; i <= 23; i++) a[i] = a[i] ^ N[i - 18];
    for (int i = 22; i <= 23; i++) {
        int a_bit = mzd_read_bit(a_vec, 0, i);
        int n_bit = mzd_read_bit(nonce_vec, 0, i - 18);
        mzd_write_bit(a_vec, 0, i, a_bit ^ n_bit);
    }
    // for (int i = 60; i <= 63; i++) a[i] = a[i] ^ N[i - 60];
    for (int i = 60; i <= 63; i++) {
        int a_bit = mzd_read_bit(a_vec, 0, i);
        int n_bit = mzd_read_bit(nonce_vec, 0, i - 60);
        mzd_write_bit(a_vec, 0, i, a_bit ^ n_bit);
    }
}

// 비트 리버설: a_vec(1x64) -> aa_vec(1x64)
void bit_reversal_m4ri(const mzd_t* a_vec, mzd_t* aa_vec) {
    // 4블록(16비트씩) 단위로 비트 순서 뒤집기
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 16; j++) {
            int bit = mzd_read_bit(a_vec, 0, i * 16 + j);
            mzd_write_bit(aa_vec, 0, i * 16 + 15 - j, bit);
        }
    }
}

// LFSR 상태 초기화: lfsr_matrix_state_t* state
void lfsr_matrix_initialization(lfsr_matrix_state_t* state) {
    if (!state) return;
    // 기존 메모리 해제 및 NULL 대입
    if (state->R1) { mzd_free(state->R1); state->R1 = NULL; }
    if (state->R2) { mzd_free(state->R2); state->R2 = NULL; }
    if (state->R3) { mzd_free(state->R3); state->R3 = NULL; }
    if (state->R4) { mzd_free(state->R4); state->R4 = NULL; }
    // my_encrypt와 동일하게 길이/피드백/마스크 의미를 명확히 주석으로 남김
    // R1: len=19, fp=0xE4000, ff=0x7FFFF
    // R2: len=22, fp=0x622000, ff=0x3FFFFF
    // R3: len=23, fp=0xCC0000, ff=0x7FFFFF
    // R4: len=17, fp=0x26200, ff=0x1FFFF
    state->R1 = mzd_init(1, 19);
    state->R2 = mzd_init(1, 22);
    state->R3 = mzd_init(1, 23);
    state->R4 = mzd_init(1, 17);
    for (int i = 0; i < 19; i++) mzd_write_bit(state->R1, 0, i, 0);
    for (int i = 0; i < 22; i++) mzd_write_bit(state->R2, 0, i, 0);
    for (int i = 0; i < 23; i++) mzd_write_bit(state->R3, 0, i, 0);
    for (int i = 0; i < 17; i++) mzd_write_bit(state->R4, 0, i, 0);
}

// 키 인젝션: aa_vec(1x64), state -> state
void key_injection_m4ri(const mzd_t* aa_vec, lfsr_matrix_state_t* state) {
    // 전역 행렬이 초기화되지 않았으면 초기화
    if (!A1 || !A2 || !A3 || !A4) {
        lfsr_matrices_init();
    }
    
    for (int k = 0; k < 64; k++) {
        lfsr_matrix_clock(state->R1, A1);
        lfsr_matrix_clock(state->R2, A2);
        lfsr_matrix_clock(state->R3, A3);
        lfsr_matrix_clock(state->R4, A4);
        int aa_bit = mzd_read_bit(aa_vec, 0, k);
        if (aa_bit) {
            // 각 LFSR의 0번째 비트에 1 XOR
            mzd_write_bit(state->R1, 0, 0, mzd_read_bit(state->R1, 0, 0) ^ 1);
            mzd_write_bit(state->R2, 0, 0, mzd_read_bit(state->R2, 0, 0) ^ 1);
            mzd_write_bit(state->R3, 0, 0, mzd_read_bit(state->R3, 0, 0) ^ 1);
            mzd_write_bit(state->R4, 0, 0, mzd_read_bit(state->R4, 0, 0) ^ 1);
        }
    }
    // 마지막에 각 LFSR의 0번째 비트를 1로 강제
    mzd_write_bit(state->R1, 0, 0, 1);
    mzd_write_bit(state->R2, 0, 0, 1);
    mzd_write_bit(state->R3, 0, 0, 1);
    mzd_write_bit(state->R4, 0, 0, 1);
}

// 상태 확장: S0_state, num, S_states[num] (선형화된 버전으로 대체)
void expand_states_from_initial_m4ri(const lfsr_matrix_state_t* S0, int num, lfsr_matrix_state_t** S_states) {
    // 선형화된 확장 함수를 호출
    expand_states_linearized_m4ri(S0, num, S_states);
}

// LSB 정규화 함수
void normalize_lsb(lfsr_matrix_state_t* state) {
    if (!state) return;
    mzd_write_bit(state->R1, 0, 0, 1);
    mzd_write_bit(state->R2, 0, 0, 1);
    mzd_write_bit(state->R3, 0, 0, 1);
    mzd_write_bit(state->R4, 0, 0, 1);
}

// keystream 생성: state, pattern -> z_vec(1x208)
void keystream_generation_with_pattern_m4ri(const lfsr_matrix_state_t* state, const uint8_t* pattern, mzd_t* z_vec) {
    // 전역 행렬이 초기화되지 않았으면 초기화
    if (!A1 || !A2 || !A3) {
        lfsr_matrices_init();
    }
    
    const int discard = 250;
    mzd_t* R1 = mzd_copy(NULL, state->R1);
    mzd_t* R2 = mzd_copy(NULL, state->R2);
    mzd_t* R3 = mzd_copy(NULL, state->R3);
    for (int i = 0; i < discard + 208; i++) {
        uint8_t p = pattern[i];
        if (p & 0b100) lfsr_matrix_clock(R1, A1);
        if (p & 0b010) lfsr_matrix_clock(R2, A2);
        if (p & 0b001) lfsr_matrix_clock(R3, A3);
        if (i >= discard) {
            int maj1 = majority_matrix(lfsr_matrix_get(R1, 1), lfsr_matrix_get(R1, 6), lfsr_matrix_get(R1, 15));
            int maj2 = majority_matrix(lfsr_matrix_get(R2, 3), lfsr_matrix_get(R2, 8), lfsr_matrix_get(R2, 14));
            int maj3 = majority_matrix(lfsr_matrix_get(R3, 4), lfsr_matrix_get(R3, 15), lfsr_matrix_get(R3, 19));
            int z_bit = maj1 ^ maj2 ^ maj3 ^ lfsr_matrix_get(R1, 11) ^ lfsr_matrix_get(R2, 1) ^ lfsr_matrix_get(R3, 0);
            mzd_write_bit(z_vec, 0, i - discard, z_bit);
        }
    }
    printf("[m4ri] z first 10 bits: ");
    for (int i = 0; i < 10; i++) printf("%d", mzd_read_bit(z_vec, 0, i));
    printf("\n");
    mzd_free(R1); mzd_free(R2); mzd_free(R3);
}

// --- 신뢰 가능한 인코딩 파트: 평문 -> e_all ---
void encoding_part_m4ri(const char* plaintext, int* e_all, int num_blocks, const int* Gt) {
    encode_plaintext_m4ri(plaintext, e_all, num_blocks, Gt);
}

// --- 전역 행렬 초기화 함수 ---
void lfsr_matrices_init(void) {
    // A 행렬들 초기화
    if (!A1) A1 = lfsr_companion_matrix_transposed(0xE4000, 19);
    if (!A2) A2 = lfsr_companion_matrix_transposed(0x622000, 22);
    if (!A3) A3 = lfsr_companion_matrix_transposed(0xCC0000, 23);
    if (!A4) A4 = lfsr_companion_matrix_transposed(0x26200, 17);
    
    // zS 행렬들 초기화 (한 번만)
    if (!zS_R1) {
        FILE* fzS = fopen("data/zS.bin", "rb");
        if (!fzS) {
            fprintf(stderr, "[m4ri] Failed to open data/zS.bin\n");
            abort();
        }
        
        // zS 행렬들 할당
        zS_R1 = mzd_init(ZS_ROWS, 19);
        zS_R2 = mzd_init(ZS_ROWS, 22);
        zS_R3 = mzd_init(ZS_ROWS, 23);
        zS_R4 = mzd_init(ZS_ROWS, 17);
        
        // zS.bin에서 비트 읽어서 행렬로 설정
        for (int i = 0; i < ZS_ROWS; i++) {
            // R1 (18 bits, excluding LSB)
            for (int j = 1; j < 19; j++) {
                int bit = fgetc(fzS);
                mzd_write_bit(zS_R1, i, j, bit);
            }
            // R2 (21 bits, excluding LSB)
            for (int j = 1; j < 22; j++) {
                int bit = fgetc(fzS);
                mzd_write_bit(zS_R2, i, j, bit);
            }
            // R3 (22 bits, excluding LSB)
            for (int j = 1; j < 23; j++) {
                int bit = fgetc(fzS);
                mzd_write_bit(zS_R3, i, j, bit);
            }
            // R4 (16 bits, excluding LSB)
            for (int j = 1; j < 17; j++) {
                int bit = fgetc(fzS);
                mzd_write_bit(zS_R4, i, j, bit);
            }
            
            // LSB는 0으로 설정 (나중에 정규화)
            mzd_write_bit(zS_R1, i, 0, 0);
            mzd_write_bit(zS_R2, i, 0, 0);
            mzd_write_bit(zS_R3, i, 0, 0);
            mzd_write_bit(zS_R4, i, 0, 0);
        }
        fclose(fzS);
        printf("[m4ri] LFSR companion matrices and zS matrices initialized\n");
    }
}

void lfsr_matrices_cleanup(void) {
    if (A1) { mzd_free(A1); A1 = NULL; }
    if (A2) { mzd_free(A2); A2 = NULL; }
    if (A3) { mzd_free(A3); A3 = NULL; }
    if (A4) { mzd_free(A4); A4 = NULL; }
    
    if (zS_R1) { mzd_free(zS_R1); zS_R1 = NULL; }
    if (zS_R2) { mzd_free(zS_R2); zS_R2 = NULL; }
    if (zS_R3) { mzd_free(zS_R3); zS_R3 = NULL; }
    if (zS_R4) { mzd_free(zS_R4); zS_R4 = NULL; }
    
    printf("[m4ri] LFSR companion matrices and zS matrices cleaned up\n");
}

// --- LFSR companion matrix 생성 ---
mzd_t* lfsr_companion_matrix_transposed(uint32_t fp, int len) {
    mzd_t* A = lfsr_companion_matrix(fp, len);
    mzd_t* AT = mzd_init(len, len);
    for (int i = 0; i < len; i++)
        for (int j = 0; j < len; j++)
            mzd_write_bit(AT, i, j, mzd_read_bit(A, j, i));
    mzd_free(A);
    return AT;
}

// --- LFSR clock: 행렬곱 기반 ---
void lfsr_matrix_clock(mzd_t* lfsr, mzd_t* A) {
    mzd_t* tmp = mzd_init(1, lfsr->ncols);
    mzd_mul(tmp, lfsr, A, 0);
    mzd_copy(lfsr, tmp);
    mzd_free(tmp);
}

int lfsr_matrix_get(const mzd_t* lfsr, int idx) {
    return mzd_read_bit(lfsr, 0, idx);
}

int majority_matrix(int a, int b, int c) {
    return (a & b) ^ (b & c) ^ (c & a);
}

// 기타 유틸리티 함수 등 필요시 추가 

mzd_t* lfsr_companion_matrix(uint32_t fp, int len) {
    mzd_t* A = mzd_init(len, len);
    for (int j = 0; j < len; j++) {
        if (j == len - 1) {
            mzd_write_bit(A, 0, j, 1);
        } else {
            mzd_write_bit(A, 0, j, (fp >> (j + 1)) & 1);
        }
    }
    for (int i = 1; i < len; i++) {
        mzd_write_bit(A, i, i - 1, 1);
    }
    return A;
}

// --- m4ri 기반: LFSR 초기상태부터 모든 블록 암호화 (my_encrypt_from_state_precise와 동일 시그니처) ---
void encrypt_from_state_precise_m4ri(
    int R1_init, int R2_init, int R3_init, int R4_init,
    const char* plaintext, int err1, int err2, int err1_bit, int err2_bit, int* c_out,
    const int* s, const int* Gt, int* z_out
) {
    const int num = NUM_BLOCKS;
    int S0[4] = {R1_init, R2_init, R3_init, R4_init};
    lfsr_matrix_state_t S0_state = {NULL, NULL, NULL, NULL};
    S0_state.R1 = mzd_init(1, 19);
    S0_state.R2 = mzd_init(1, 22);
    S0_state.R3 = mzd_init(1, 23);
    S0_state.R4 = mzd_init(1, 17);
    for (int i = 0; i < 19; i++) mzd_write_bit(S0_state.R1, 0, i, (S0[0] >> i) & 1);
    for (int i = 0; i < 22; i++) mzd_write_bit(S0_state.R2, 0, i, (S0[1] >> i) & 1);
    for (int i = 0; i < 23; i++) mzd_write_bit(S0_state.R3, 0, i, (S0[2] >> i) & 1);
    for (int i = 0; i < 17; i++) mzd_write_bit(S0_state.R4, 0, i, (S0[3] >> i) & 1);
    lfsr_matrix_state_t* S_states[num];
    expand_states_from_initial_m4ri(&S0_state, num, S_states);
    int e_all[num * CIPHERTEXT_SIZE];
    encoding_part_m4ri(plaintext, e_all, num, Gt);
    static uint8_t* clock_patterns = NULL;
    static int patterns_loaded = 0;
    const int CLOCK_PATTERN_LEN = 458;
    const int CLOCK_PATTERN_STATES = 65536;
    if (!patterns_loaded) {
        clock_patterns = (uint8_t*)malloc(CLOCK_PATTERN_STATES * CLOCK_PATTERN_LEN);
        FILE* f = fopen("data/r4_clock_patterns.bin", "rb");
        if (f && clock_patterns) {
            fread(clock_patterns, 1, CLOCK_PATTERN_STATES * CLOCK_PATTERN_LEN, f);
            fclose(f);
            patterns_loaded = 1;
        }
    }
    for (int i = 0; i < num; i++) {
        lfsr_matrix_state_t* state = S_states[i];
        if (!state || !state->R1 || !state->R2 || !state->R3 || !state->R4) {
            fprintf(stderr, "[m4ri] S_states[%d] or internal R1~R4 is NULL.\n", i);
            abort();
        }
        int r4_index = 0;
        for (int k = 1; k < 17; k++) {
            r4_index |= (mzd_read_bit(state->R4, 0, k) << (k - 1));
        }
        if (!clock_patterns) { fprintf(stderr, "[m4ri] clock_patterns is NULL\n"); abort(); }
        if (r4_index < 0 || r4_index >= CLOCK_PATTERN_STATES) {
            fprintf(stderr, "[m4ri] r4_index(%d) out of range\n", r4_index); abort();
        }
        mzd_t* keystream_vec = mzd_init(1, CIPHERTEXT_SIZE);
        keystream_generation_with_pattern_m4ri(state, &clock_patterns[r4_index * CLOCK_PATTERN_LEN], keystream_vec);
        mzd_t* c_vec = mzd_init(1, CIPHERTEXT_SIZE);
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            int ebit = e_all[i * CIPHERTEXT_SIZE + j];
            int zbit = mzd_read_bit(keystream_vec, 0, j);
            int sbit = s[j];
            mzd_write_bit(c_vec, 0, j, ebit ^ zbit ^ sbit);
        }
        if (err1 == i && err1_bit < CIPHERTEXT_SIZE) {
            int bit = mzd_read_bit(c_vec, 0, err1_bit);
            mzd_write_bit(c_vec, 0, err1_bit, bit ^ 1);
        }
        if (err2 == i && err2_bit < CIPHERTEXT_SIZE) {
            int bit = mzd_read_bit(c_vec, 0, err2_bit);
            mzd_write_bit(c_vec, 0, err2_bit, bit ^ 1);
        }
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            c_out[i * CIPHERTEXT_SIZE + j] = mzd_read_bit(c_vec, 0, j);
        }
        
        // 키스트림을 z_out에 복사
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            z_out[i * CIPHERTEXT_SIZE + j] = mzd_read_bit(keystream_vec, 0, j);
        }
        
        mzd_free(keystream_vec);
        mzd_free(c_vec);
    }
    if (S0_state.R1) mzd_free(S0_state.R1);
    if (S0_state.R2) mzd_free(S0_state.R2);
    if (S0_state.R3) mzd_free(S0_state.R3);
    if (S0_state.R4) mzd_free(S0_state.R4);
}

// --- 선형화된 상태 확장 (zS.bin 사용) ---
void expand_states_linearized_m4ri(const lfsr_matrix_state_t* S0, int num, lfsr_matrix_state_t** S_states) {
    // 전역 zS가 초기화되지 않았으면 초기화
    if (!zS_R1) {
        lfsr_matrices_init();
    }
    
    // 각 S_states 생성 (진짜 행렬 연산)
    for (int i = 0; i < num; i++) {
        S_states[i] = malloc(sizeof(lfsr_matrix_state_t));
        if (!S_states[i]) { fprintf(stderr, "[m4ri] S_states[%d] malloc failed\n", i); abort(); }
        
        S_states[i]->R1 = mzd_init(1, 19);
        S_states[i]->R2 = mzd_init(1, 22);
        S_states[i]->R3 = mzd_init(1, 23);
        S_states[i]->R4 = mzd_init(1, 17);
        
        if (!S_states[i]->R1 || !S_states[i]->R2 || !S_states[i]->R3 || !S_states[i]->R4) {
            fprintf(stderr, "[m4ri] S_states[%d] internal R1~R4 mzd_init failed\n", i); abort();
        }
        
        if (i == 0) {
            // S[0] = S0
            mzd_copy(S_states[i]->R1, S0->R1);
            mzd_copy(S_states[i]->R2, S0->R2);
            mzd_copy(S_states[i]->R3, S0->R3);
            mzd_copy(S_states[i]->R4, S0->R4);
        } else {
            // S[i] = S0 ⊕ zS[i-1] (진짜 행렬 연산)
            // zS의 (i-1)번째 row를 추출하여 S0와 XOR
            
            // R1: zS_R1의 (i-1)번째 row를 추출
            mzd_t* zS_row_R1 = mzd_init_window(zS_R1, i-1, 0, i, 19);
            mzd_add(S_states[i]->R1, S0->R1, zS_row_R1);
            mzd_free(zS_row_R1);
            
            // R2: zS_R2의 (i-1)번째 row를 추출  
            mzd_t* zS_row_R2 = mzd_init_window(zS_R2, i-1, 0, i, 22);
            mzd_add(S_states[i]->R2, S0->R2, zS_row_R2);
            mzd_free(zS_row_R2);
            
            // R3: zS_R3의 (i-1)번째 row를 추출
            mzd_t* zS_row_R3 = mzd_init_window(zS_R3, i-1, 0, i, 23);
            mzd_add(S_states[i]->R3, S0->R3, zS_row_R3);
            mzd_free(zS_row_R3);
            
            // R4: zS_R4의 (i-1)번째 row를 추출
            mzd_t* zS_row_R4 = mzd_init_window(zS_R4, i-1, 0, i, 17);
            mzd_add(S_states[i]->R4, S0->R4, zS_row_R4);
            mzd_free(zS_row_R4);
        }
        
        // LSB 정규화 (별도 함수)
        normalize_lsb(S_states[i]);
    }
}

// m4ri 기반 keystream 생성 함수 (검증된 함수들 재사용)
void lfsr_enc_m4ri(const int K[KEY_SIZE], const int N[NONCE_SIZE], int z[CIPHERTEXT_SIZE]) {
    mzd_t* key_vec = mzd_init(1, KEY_SIZE);
    mzd_t* nonce_vec = mzd_init(1, NONCE_SIZE);
    for (int i = 0; i < KEY_SIZE; i++) mzd_write_bit(key_vec, 0, i, K[i]);
    for (int i = 0; i < NONCE_SIZE; i++) mzd_write_bit(nonce_vec, 0, i, N[i]);
    mzd_t* a_vec = mzd_init(1, KEY_SIZE);
    key_scheduling_m4ri(key_vec, nonce_vec, a_vec);
    mzd_t* aa_vec = mzd_init(1, KEY_SIZE);
    bit_reversal_m4ri(a_vec, aa_vec);
    lfsr_matrix_state_t state = {NULL, NULL, NULL, NULL};
    lfsr_matrix_initialization(&state);
    key_injection_m4ri(aa_vec, &state);
    int r4_index = 0;
    for (int k = 1; k < 17; k++) {
        r4_index |= (mzd_read_bit(state.R4, 0, k) << (k - 1));
    }
    static uint8_t* clock_patterns = NULL;
    static int patterns_loaded = 0;
    const int CLOCK_PATTERN_LEN = 458;
    const int CLOCK_PATTERN_STATES = 65536;
    if (!patterns_loaded) {
        clock_patterns = (uint8_t*)malloc(CLOCK_PATTERN_STATES * CLOCK_PATTERN_LEN);
        FILE* f = fopen("data/r4_clock_patterns.bin", "rb");
        if (f && clock_patterns) {
            fread(clock_patterns, 1, CLOCK_PATTERN_STATES * CLOCK_PATTERN_LEN, f);
            fclose(f);
            patterns_loaded = 1;
        }
    }
    mzd_t* z_vec = mzd_init(1, CIPHERTEXT_SIZE);
    keystream_generation_with_pattern_m4ri(&state, &clock_patterns[r4_index * CLOCK_PATTERN_LEN], z_vec);
    for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
        z[j] = mzd_read_bit(z_vec, 0, j);
    }
    mzd_free(z_vec);
    mzd_free(key_vec);
    mzd_free(nonce_vec);
    mzd_free(a_vec);
    mzd_free(aa_vec);
    mzd_free(state.R1);
    mzd_free(state.R2);
    mzd_free(state.R3);
    mzd_free(state.R4);
}

// m4ri 기반: my_encrypt와 동일한 시그니처의 암호화 함수
void encrypt_m4ri(const int key[KEY_SIZE], const char* plaintext, int err1, int err2, int err1_bit, int err2_bit, int* ciphertext, const int* s, const int* Gt) {
    const int num = NUM_BLOCKS;
    int FN = 9867;
    int N[num][NONCE_SIZE];
    int p[num][PLAINTEXT_BLOCK_SIZE];
    int e[num][CIPHERTEXT_SIZE];
    int z[num][CIPHERTEXT_SIZE];
    int c[num][CIPHERTEXT_SIZE];

    // 1. nonce 생성 (my_encrypt와 동일)
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < NONCE_SIZE; j++) {
            N[i][j] = ((FN + i) >> j) & 1;
        }
    }

    // 2. 평문을 비트로 변환 (my_encrypt와 동일)
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < 20; j++) {
            unsigned char w = (unsigned char)plaintext[i * 20 + j];
            for (int k = 0; k < 8; k++) {
                p[i][j * 8 + k] = (w >> (7 - k)) & 1;
            }
        }
    }

    // 3. e 계산 (my_encrypt와 동일)
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            e[i][j] = 0;
            for (int k = 0; k < PLAINTEXT_BLOCK_SIZE; k++) {
                e[i][j] ^= Gt[j*PLAINTEXT_BLOCK_SIZE + k] & p[i][k];
            }
        }
    }

    // 4. 각 블록별로 키스트림 생성 (기존 lfsr_enc 함수 재사용)
    for (int i = 0; i < num; i++) {
        lfsr_enc_m4ri(key, N[i], z[i]);
    }

    // 5. 암호문 결합 (my_encrypt와 동일)
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            c[i][j] = e[i][j] ^ z[i][j] ^ s[j];
        }
    }

    // 6. 에러 비트 적용 (my_encrypt와 동일)
    if (err1 >= 0 && err1 < NUM_BLOCKS && err1_bit >= 0 && err1_bit < CIPHERTEXT_SIZE)
        c[err1][err1_bit] ^= 1;
    if (err2 >= 0 && err2 < NUM_BLOCKS && err2_bit >= 0 && err2_bit < CIPHERTEXT_SIZE)
        c[err2][err2_bit] ^= 1;

    // 7. 결과를 1차원 배열로 복사 (my_encrypt와 동일)
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            ciphertext[i * CIPHERTEXT_SIZE + j] = c[i][j];
        }
    }
} 
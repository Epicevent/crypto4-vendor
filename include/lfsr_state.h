#ifndef LFSR_STATE_H
#define LFSR_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <m4ri/m4ri.h>

// ── 상수 정의 ────────────────────────────────────────────────────────────
#define KEY_SIZE                64
#define NONCE_SIZE              19
#define PLAINTEXT_BLOCK_SIZE    160
#define CIPHERTEXT_SIZE         208
#define NUM_BLOCKS              15
#define ZS_ROWS                 14

/// 패턴 수
#define CLOCK_PATTERN_STATES    65536
/// 각 패턴 길이
#define CLOCK_PATTERN_LEN       458
/// LFSR 전체 변수 개수 (1×656)
#define TOTAL_VARS              656

#define DISCARD                 250
/// 패턴 파일 경로
#define CLOCK_PATTERNS_FILE     "data/r4_clock_patterns.bin"

// ── m4ri 기반 LFSR 상태 구조체 ───────────────────────────────────────────
typedef struct {
    mzd_t *R1;   // 1×19
    mzd_t *R2;   // 1×22
    mzd_t *R3;   // 1×23
    mzd_t *R4;   // 1×17
    mzd_t *v;    // 1×656 변수 벡터
} lfsr_matrix_state_t;

// ── Clock patterns ───────────────────────────────────────────────────────
void init_clock_patterns(void);
const uint8_t *get_clock_pattern(uint16_t r4_index);
void cleanup_clock_patterns(void);

// ── LFSR companion matrices & clocking ───────────────────────────────────
mzd_t *lfsr_companion_matrix(uint32_t feedback_poly, int len);
mzd_t *lfsr_companion_matrix_transposed(uint32_t feedback_poly, int len);
void lfsr_matrix_clock(mzd_t *lfsr, mzd_t *A);
int  lfsr_matrix_get(const mzd_t *lfsr, int idx);
int  majority_matrix(int a, int b, int c);

// ── 전역 행렬 초기화/해제 ────────────────────────────────────────────────
void lfsr_matrices_init(void);
void lfsr_matrices_cleanup(void);

// ── 상태 초기화 헬퍼 ────────────────────────────────────────────────────
void lfsr_matrix_initialization(lfsr_matrix_state_t *state);

#endif // LFSR_STATE_H

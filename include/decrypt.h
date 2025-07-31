// File: decrypt.h
#ifndef DECRYPT_H
#define DECRYPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


#include "lfsr_state.h"  // defines lfsr_matrix_state_t, extract_variables_from_state

// constants for variable layout
#define C_ROWS     208
#define CONSTANT_TERM_INDEX 0
#define VAR_LEN_R1 171 // 19 + 18 + 17 + ... + 1 = 171
#define VAR_OFF_R1 1 // 0th index is constant term
#define VAR_LEN_R2 231 // 22 + 21 + 20 + ... + 1 = 231
#define VAR_OFF_R2 (VAR_OFF_R1 + VAR_LEN_R1)
#define VAR_LEN_R3 253 // 23 + 22 + 21 + ... + 1 = 253
#define VAR_OFF_R3 (VAR_OFF_R2 + VAR_LEN_R2)
#define BLOCK_BYTES 26

#define CIPHERTEXT_PATH  "data/ciphertext.bin"
#define SCRAMBLE_PATH    "data/s.bin"
#define V_DIFF_COUNT  ZS_ROWS      // zS가 제공하는 차분 행 수 (14)
#define V_DIFF_SIZE   TOTAL_VARS   // v 벡터 차원 (656)
#define R4_SPACE  (1<<16)
// 기존 extern 선언들…
extern mzd_t *H, *Ht;
extern mzd_t *CtHt_cache[R4_SPACE];
extern mzd_t *c_vecs[NUM_BLOCKS];
extern mzd_t *cHt_vecs[NUM_BLOCKS];
extern mzd_t  *V_DIFF_MATS[V_DIFF_COUNT];
// 서브시스템 초기화/해제 함수 선언
void init_H(void);
void free_H(void);
void init_c_vecs(void);
// In decrypt.h (ensure declaration is present)
void init_v_diff_matrices(void);
void free_v_diff_matrices(void);


void init_CtHt_cache(void);
void free_CtHt_cache(void);

void init_cHt_vecs(void);
void free_cHt_vecs(void);

void init_v_diff_matrices(void);
void free_v_diff_matrices(void);

// **전체 전역 상태** 초기화/해제
void init_globals(void);
void free_globals(void);

unsigned char *load_packed_bin(const char *path, size_t *out_bytes  );
mzd_t *load_packed_matrix(const char *path, int rows, int cols);
//------------------------------------------------------------------------------
// LSegment: holds intermediate state for one LFSR register
//------------------------------------------------------------------------------
typedef struct {
    uint16_t      var_offset;  // offset into the full 655‑var space (for labeling)
    uint16_t      var_len;     // number of vars for this reg
    uint16_t      reg_len;     // bit‑length of this LFSR (19/22/23)
    uint8_t       r;           // register ID: 1,2,3

    mzd_t*        L;           // reg_len×4 basis matrix E or A^k·E
    mzd_t*        row;         // 1×TOTAL_VARS accumulated coefficients
} LSegment;
// 전역 변환 행렬 배열: 블럭 i(1..14)에 대응
extern mzd_t *V_DIFF_MATS[V_DIFF_COUNT];

/**
 * @brief V_DIFF_MATS를 초기화합니다.
 * - lfsr_matrices_init() 으로 zS_R1..R4를 로드
 * - 각 V_DIFF_MATS[i]에 656×656 단위행렬을 만들고,
 *   zS_R1..R3 행(i)에서 가져온 1차차분 및 2차차분을
 *   해당 변환행렬의 열로 반영합니다.
 */
void init_v_diff_matrices(void);

/**
 * @brief V_DIFF_MATS에 할당된 모든 행렬을 해제합니다.
 */
void free_v_diff_matrices(void);
//------------------------------------------------------------------------------
// Prototypes for functions in decrypt.c
//------------------------------------------------------------------------------
void compute_segment_row(LSegment* seg);
//------------------------------------------------------------------------------
void init_LSegment(LSegment* seg,
                   uint16_t var_offset,
                   uint16_t var_len,
                   uint8_t  r
                 );
void update_LSegment(LSegment* seg);
void free_LSegment(LSegment* seg);

/**
 * Build the linear system on‑the‑fly:
 *  - dcol: 208×1 column of constant terms
 */
void build_linear_system_with_pattern(
    const uint8_t* pattern,
    mzd_t*        C
);


/**
 * Generate the keystream z_vec using the linear system and current LFSR state.
 */
void generate_keystream_via_linear_system(lfsr_matrix_state_t* state,
                                          mzd_t*               z_vec);

int find_first_one_from_1(const mzd_t* row) ;

void print_first_one_from_1(const mzd_t* row);

// r × 4 행렬 L에서 3탭 페어만 사용해 r × r 행렬을 계산.
// 반환된 행렬은 호출자 책임으로 mzd_free 해야 함.
mzd_t* compute_pairwise_matrix(const mzd_t* L);
 
void load_cipher_noscramble_m4ri(
    const char* cipher_path,
    const char* scramble_path,
    mzd_t*      c_vecs[NUM_BLOCKS]
);

int test_ct_build(void);

#endif // DECRYPT_H

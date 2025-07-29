// File: decrypt.h
#ifndef DECRYPT_H
#define DECRYPT_H

#include <stdint.h>
#include <m4ri/m4ri.h>
#include "encrypt.h"  // defines lfsr_matrix_state_t, extract_variables_from_state

// constants for variable layout
#define TOTAL_VARS 656
#define DISCARD    250
#define CONSTANT_TERM_INDEX 0
#define VAR_LEN_R1 171 // 19 + 18 + 17 + ... + 1 = 171
#define VAR_OFF_R1 1 // 0th index is constant term
#define VAR_LEN_R2 231 // 22 + 21 + 20 + ... + 1 = 231
#define VAR_OFF_R2 (VAR_OFF_R1 + VAR_LEN_R1)
#define VAR_LEN_R3 253 // 23 + 22 + 21 + ... + 1 = 253
#define VAR_OFF_R3 (VAR_OFF_R2 + VAR_LEN_R2)

// External global transition matrices
extern mzd_t *A1, *A2, *A3;

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
#endif // DECRYPT_H

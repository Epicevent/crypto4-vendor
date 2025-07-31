#ifndef ERROR_BITS_H
#define ERROR_BITS_H

#include "decrypt.h"

/* 최대 블록 수는 외부에서 정의되어야 합니다. */
#ifndef NUM_BLOCKS
#error "NUM_BLOCKS must be defined before including error_bits.h"
#endif

/* 블록당 최대 추적할 수 있는 오류 비트 수 */
#define MAX_ERROR_BITS_PER_BLOCK 2

/* 블록 단위 오류 상태 */
typedef enum {
    BLOCK_NO_ERROR = 0,        /* 해당 블록에 오류 없음 */
    BLOCK_ERROR_UNKNOWN_POS,   /* 오류는 있으나 위치 미확인 */
    BLOCK_ERROR_KNOWN_POS      /* 오류 위치까지 모두 확인됨 */
} block_error_status_t;

/* 개별 블록의 오류 정보 */
typedef struct {
    block_error_status_t status;
    int                     error_bits_count;                  /* 알려진 오류 비트 개수 (0 ~ MAX_ERROR_BITS_PER_BLOCK) */
    int                     error_position;
                                                               /* 오류 비트 인덱스 (0-based, 블록 내 비트 위치) */
    mzd_t*                  syndrome;                          /* 이 블록의 시냅스 행렬 */
} block_error_t;

/* 전체 블록 오류 집합 */
typedef struct {
    block_error_t blocks[NUM_BLOCKS];
} error_bits_t;

typedef struct {
    error_bits_t *list;
    size_t        count;
} error_config_list_t;
typedef struct {
    mzd_t    *A_tr;    // RREF된 Aᵀ (n×m)
    uint32_t pivots[672]; // 최대 pivots
    rci_t     npiv;    // pivots 길이
} solver_ctx_t;

void populate_error_config_syndromes(error_config_list_t *configs);

void generate_error_configs(error_config_list_t *configs);
bool check_solvability_incremental(mzd_t *A, mzd_t *b);


/**
 * @brief  Prepare the solver context: compute full RREF(Aᵀ) and extract pivots.
 * @param  A  m×n matrix
 * @return    할당된 solver_ctx_t*, 사용 후 solver_free로 해제해야 함
 */
solver_ctx_t *solver_prepare(const mzd_t *A);

/**
 * @brief  Prepare 없이 이미 컨텍스트 갖고 있다면 solver_prepare를 건너뛰고
 *         직접 ctx를 넘겨 받을 수 있는 checker.
 * @param  ctx  solver_prepare가 만든 context
 * @param  b    m×1 vector
 * @return      true iff A·x=b has a solution
 */
bool solver_check(const solver_ctx_t *ctx, const mzd_t *b);

/**
 * @brief  solver_prepare로 할당된 리소스 해제
 * @param  ctx
 */
void solver_free(solver_ctx_t *ctx);

void assemble_system(uint16_t R4,
                     mzd_t *A_list[NUM_BLOCKS],
                     mzd_t *b_list[NUM_BLOCKS]); 
/**
 * @brief   Given a set of per‐block coefficient matrices A_list (and optional b_list),
 *          build the concatenated “global” A matrix for a specified unknown block.
 *
 * @param   A_list    An array of NUM_BLOCKS pointers to already‐assembled block matrices
 *                    (each of dimension H_rows×H_cols, e.g. 48×655).
 * @param   unknown   Index of the block to treat as “unknown” (0 … NUM_BLOCKS−1);
 *                    A_list[unknown] will be omitted.
 * @param   A_out     Output pointer; on return *A_out is the stacked matrix of all
 *                    A_list[j] for j≠unknown (dimension (NUM_BLOCKS−1)*H_rows × H_cols).
 *                    Caller must mzd_free(*A_out).
 */
void assemble_A_for_unknown(const mzd_t *A_list[NUM_BLOCKS],
                            int          unknown,
                            mzd_t       **A_out);

                           // assemble_A_for_unknowns_2_input  (A_list, unknown1, unknown2, &A_large);
                        
void assemble_A_for_unknowns_2_input(const mzd_t *A_list[NUM_BLOCKS],
                                       int          unknown1,
                                       int          unknown2,
                                       mzd_t       **A_out);                           
#endif /* ERROR_BITS_H */

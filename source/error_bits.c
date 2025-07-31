#include "error_bits.h"




static void compute_block_syndrome(block_error_t *block);
#include "error_bits.h"
#include "m4ri/m4ri.h"

/**
 * @brief   Concatenate all block‐matrices except the one at index `unknown`.
 * @param   A_list    Array of NUM_BLOCKS pointers to mzd_t* (each H_rows×H_cols).
 * @param   unknown   Index to omit (0 … NUM_BLOCKS−1).
 * @param   A_out     Output: pointer to the new concatenated matrix.
 *                    Caller must mzd_free(*A_out).
 */
void assemble_A_for_unknown(const mzd_t *A_list[NUM_BLOCKS],
                            int          unknown,
                            mzd_t      **A_out)
{
    mzd_t *A = NULL;

    for (int j = 0; j < NUM_BLOCKS; ++j) {
        if (j == unknown) {
            continue;
        }
        if (A == NULL) {
            // First block: just copy
            A = mzd_copy(NULL, A_list[j]);
        } else {
            // Stack this block under existing A
            mzd_t *tmp = mzd_stack(NULL, A, A_list[j]);
            mzd_free(A);
            A = tmp;
        }
    }

    // Return the assembled matrix
    *A_out = A;
}

void assemble_system(uint16_t R4,
                     mzd_t *A_list[NUM_BLOCKS],
                     mzd_t *b_list[NUM_BLOCKS]) {

    // CtHt_cache[R4]: CtHt is 656×48
    mzd_t *CtHt = CtHt_cache[R4];

    for (int i = 0; i < NUM_BLOCKS; ++i) {
        // 1) Build S as 656×48
        mzd_t *S;
        if (i == 0) {
            // Block 0: S = CtHt
            S = mzd_copy(NULL, CtHt);
        } else {
            // Blocks 1…: S = CtHt * V_DIFF_MATS[i-1]
            //  V_DIFF_MATS[i-1] (48×48)CtHt (656×48) × → 48×656
            mzd_t *tmp = mzd_init(V_DIFF_MATS[i-1]->nrows, CtHt->ncols);
            mzd_mul_naive(tmp, V_DIFF_MATS[i-1], CtHt);  
            S = tmp;
        }

        // 2) Extract first row (1×48) into r0
        mzd_t *r0 = mzd_init(1, S->ncols);  // 1×48
        for (int c = 0; c < S->ncols; ++c) {
            mzd_write_bit(r0, 0, c, mzd_read_bit(S, 0, c));
        }

        mzd_add(r0, r0, cHt_vecs[i]);
        // 4) b_list[i] = transpose(r0) → 48×1
        mzd_t *b_i = mzd_transpose(NULL, r0);
        mzd_free(r0);

        // 5) A_part = rows 1…655 of S → 655×48
        mzd_t *A_part = mzd_init_window(S,
                                        1, 0,
                                        S->nrows, S->ncols);
        // 6) A_list[i] = transpose(A_part) → 48×655
        mzd_t *A_i = mzd_transpose(NULL, A_part);
        mzd_free(A_part);

        // store and clean up
        A_list[i] = A_i;
        b_list[i] = b_i;
        mzd_free(S);
    }
}
/// 호출자는 반환된 리스트를 다 쓰면 free(configs.list) 해야 합니다.
void generate_error_configs(error_config_list_t *configs){
    size_t total = NUM_BLOCKS                                  // UNKNOWN_POS 단독
                 + (size_t)NUM_BLOCKS * (NUM_BLOCKS - 1)      // UNKNOWN+KNOWN 블록 쌍
                   * CIPHERTEXT_SIZE; // 각 블록에 대해 CIPHERTEXT_SIZE 위치
    error_bits_t *arr = malloc(sizeof(error_bits_t) * total);
    size_t idx = 0;

    error_bits_t cfg ;
    // --- 1) UNKNOWN_POS 단독 ---
    for (int b1 = 0; b1 < NUM_BLOCKS; ++b1) {
        memset(&cfg, 0, sizeof(cfg));
        for (int i = 0; i < NUM_BLOCKS; ++i)
            cfg.blocks[i].status = BLOCK_NO_ERROR;
        cfg.blocks[b1].status = BLOCK_ERROR_UNKNOWN_POS;
        arr[idx++] = cfg;
        
    }

    // --- 2) UNKNOWN_POS + KNOWN_POS ---
    for (int b1 = 0; b1 < NUM_BLOCKS; ++b1) {
        for (int b2 = 0; b2 < NUM_BLOCKS; ++b2) {
            if (b2 == b1) continue;
            for (int pos = 0; pos < CIPHERTEXT_SIZE; ++pos) {
                memset(&cfg, 0, sizeof(cfg));
                for (int i = 0; i < NUM_BLOCKS; ++i)
                    cfg.blocks[i].status = BLOCK_NO_ERROR;
                cfg.blocks[b1].status         = BLOCK_ERROR_UNKNOWN_POS;
                cfg.blocks[b2].status         = BLOCK_ERROR_KNOWN_POS;
                cfg.blocks[b2].error_position = pos;
                arr[idx++] = cfg;
            }
        }
    }

    configs->list  = arr;
    configs->count = idx;
}


void populate_error_config_syndromes(error_config_list_t *configs) {
    for (size_t i = 0; i < configs->count; ++i) {
        for (int b = 0; b < NUM_BLOCKS; ++b) {
            compute_block_syndrome(&configs->list[i].blocks[b]);
        }
    }
}


/**
 * @brief 단일 block_error_t의 syndrome 벡터를 초기화 및 계산
 * @param block 계산할 block_error_t 포인터
 */
static void compute_block_syndrome(block_error_t *block) {
    if (block == NULL) {
        fprintf(stderr, "compute_block_syndrome: block must not be NULL\n");
        return;
    }

    if (block->status == BLOCK_ERROR_UNKNOWN_POS ) {
        if (block->syndrome) {
            mzd_free(block->syndrome);
            block->syndrome = NULL;
        }
        return;
    }

    mzd_t* Global_Parrity_Matrix = NULL;
    if (H == NULL) {
        init_H();
    }
    Global_Parrity_Matrix = H;

    if (block->syndrome) {
        mzd_free(block->syndrome);
    }
    else {
        block->syndrome = mzd_init(Global_Parrity_Matrix->nrows, 1);
    }

    if ( block->status == BLOCK_NO_ERROR){
        return;
    }
    // BLOCK_ERROR_KNOWN_POS

    if (block->status != BLOCK_ERROR_KNOWN_POS) {
        fprintf(stderr, "compute_block_syndrome: invalid block status %d\n", block->status);
        return;
    }


    if (block->error_position < 0 || block->error_position >= CIPHERTEXT_SIZE) {
        fprintf(stderr, "compute_block_syndrome: invalid error position %d\n", block->error_position);
        return;
    }
    mzd_t* e_vec = mzd_init(CIPHERTEXT_SIZE, 1);
    mzd_write_bit(e_vec, 0, block->error_position, 1);
    mzd_mul_naive(block->syndrome, Global_Parrity_Matrix, e_vec);
}   
solver_ctx_t *solver_prepare(const mzd_t *A) {
    solver_ctx_t *ctx = malloc(sizeof *ctx);
    rci_t m = A->nrows, n = A->ncols;

    // 1) Aᵀ 전치 + full RREF
    ctx->A_tr = mzd_transpose(NULL, A);       // dims: n×m
    mzd_gauss_delayed(ctx->A_tr, 0, TRUE);    // full Gauss–Jordan

    // 2) pivot-columns 추출 (기존 구현 그대로)
    ctx->npiv = 0;
    for (rci_t r = 0; r < ctx->A_tr->nrows; ++r) {
        for (rci_t c = 0; c < ctx->A_tr->ncols; ++c) {
            if (mzd_read_bit(ctx->A_tr, r, c)) {
                ctx->pivots[ctx->npiv++] = (uint32_t)c;
                break;
            }
        }
    }
    return ctx;
}

bool solver_check(const solver_ctx_t *ctx, const mzd_t *b) {
    // 3) 기존 구현의 3)~ 끝 부분을 그대로 사용
    rci_t new_r = ctx->A_tr->nrows;
    mzd_t *b_row = mzd_transpose(NULL, b);        // 1×m
    mzd_t *M     = mzd_stack(NULL, ctx->A_tr, b_row); // (n+1)×m
    mzd_free(b_row);

    for (rci_t i = 0; i < ctx->npiv; ++i) {
        rci_t col = ctx->pivots[i];
        if (mzd_read_bit(M, new_r, col)) {
            mzd_row_add_offset(M, new_r, i, col);
        }
    }

    // original code used mzd_init_window + mzd_is_zero
    mzd_t *ret = mzd_init_window(M, new_r, 0, new_r + 1, M->ncols);
    bool solvable = mzd_is_zero(ret);
    mzd_free_window(ret);

    mzd_free(M);
    return solvable;
}

void solver_free(solver_ctx_t *ctx) {
    mzd_free(ctx->A_tr);
    free(ctx);
}

/**
 * @brief  Check if A x = b has a solution by comparing rank(A) to rank([A|b]),
 *         but doing it incrementally on b’s row only.
 * @param  A  m×n matrix
 * @param  b  m×1 vector
 * @return    true iff rank(A) == rank([A|b])
 */
/**
 * @brief  Incrementally check A·x = b solvability by skipping
 *         the full re‐elimination of A when building [A|b].
 */
bool check_solvability_incremental(mzd_t *A, mzd_t *b) {
    solver_ctx_t *ctx = solver_prepare(A);
    bool result       = solver_check(ctx, b);
    solver_free(ctx);
    return result;
}

void assemble_A_for_unknowns_2_input(const mzd_t *A_list[NUM_BLOCKS],
                                       int          unknown1,
                                       int          unknown2,
                                       mzd_t       **A_out){

    mzd_t *A = NULL;

    for (int j = 0; j < NUM_BLOCKS; ++j) {
        if (j == unknown1 || j == unknown2) {
            continue;
        }
        if (A == NULL) {
            // First block: just copy
            A = mzd_copy(NULL, A_list[j]);
        } else {
            // Stack this block under existing A
            mzd_t *tmp = mzd_stack(NULL, A, A_list[j]);
            mzd_free(A);
            A = tmp;
        }
    }

    // Return the assembled matrix
    *A_out = A;

                                       }     




#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <stdbool.h>
#include "lfsr_state.h"        // lfsr_matrices_init, lfsr_matrix_initialization_regs
#include "encrypt.h"
#include "decrypt.h"            // init_globals_for_r4
#include "error_bits.h"         // generate_error_configs, populate_error_config_syndromes


/**
 * @brief   Test whether any error‐bit configuration for a given R4 is solvable.
 * @param   R4        The R4 index to check (0 … R4_SPACE−1).
 * @param   configs   Fully populated error_config_list_t (with syndromes).
 * @return            true iff at least one configuration in configs for this R4 yields a solution.
 */


bool is_invalid_r4(uint16_t R4,
                 const error_config_list_t *configs){

    // 1) fast‐init only this R4
    if (CtHt_cache[R4] == NULL) {
        printf("warning: R4 %d not initialized      .\n", R4);
        init_globals_for_r4(R4);
        printf("warning: R4 %d initialized successfully.\n", R4);
    }
   
    // 2) build per‐block system once
    mzd_t *A_list[NUM_BLOCKS];
    mzd_t *b_base[NUM_BLOCKS];
    assemble_system(R4, A_list, b_base);

    for (int unknown1 = 0; unknown1 < NUM_BLOCKS; ++unknown1) {
        for (int unknown2 = unknown1 + 1; unknown2 < NUM_BLOCKS; ++unknown2) {
            // assemble large A and prepare solver
            mzd_t *A_large = NULL;
            assemble_A_for_unknowns_2_input  (A_list, unknown1, unknown2, &A_large);
            solver_ctx_t *ctx = solver_prepare(A_large);
                // build b by stacking per-block segments
                mzd_t *b = NULL;
                for (int j = 0; j < NUM_BLOCKS; ++j) {
                    if (j == unknown1 || j == unknown2) continue;
                    mzd_t *seg = mzd_copy(NULL, b_base[j]);
                    if (!b) {
                        b = seg;
                    } else {
                        mzd_t *tmp = mzd_stack(NULL, b, seg);
                        mzd_free(b);
                        mzd_free(seg);
                        b = tmp;
                    }
                }
                assert(b);

                // check solvability
                bool solvable = solver_check(ctx, b);
                mzd_free(b);
                if (solvable) {
                    // cleanup and return false
                    solver_free(ctx);
                    mzd_free(A_large);
                    for (int k = 0; k < NUM_BLOCKS; ++k) {
                        mzd_free(A_list[k]);
                        mzd_free(b_base[k]);
                    }
                    return false;
                }
            

            mzd_free(A_large);
        }


    }
    return true;
}

bool is_valid_r4(uint16_t R4,
                 const error_config_list_t *configs)
{
    // 1) fast‐init only this R4
    if (CtHt_cache[R4] == NULL) {
        printf("warning: R4 %d not initialized      .\n", R4);
        init_globals_for_r4(R4);
        printf("warning: R4 %d initialized successfully.\n", R4);
    }
   

    // 2) build per‐block system once
    mzd_t *A_list[NUM_BLOCKS];
    mzd_t *b_base[NUM_BLOCKS];
    assemble_system(R4, A_list, b_base);

    // 3) how many configs per unknown block
    size_t segment = 1 + (NUM_BLOCKS - 1) * CIPHERTEXT_SIZE;

    // 4) for each unknown block
    for (int unknown = 0; unknown < NUM_BLOCKS; ++unknown) {
        // assemble large A and prepare solver
        mzd_t *A_large = NULL;
        assemble_A_for_unknown(A_list, unknown, &A_large);
        solver_ctx_t *ctx = solver_prepare(A_large);

        // test each config in this unknown’s segment
        size_t start = unknown * segment;
        size_t end   = start + segment;
        for (size_t idx = start; idx < end; ++idx) {
            const error_bits_t *cfg = &configs->list[idx];

            // build b by stacking per‐block segments
            mzd_t *b = NULL;
            for (int j = 0; j < NUM_BLOCKS; ++j) {
                if (j == unknown) continue;
                mzd_t *seg = mzd_copy(NULL, b_base[j]);
                if (cfg->blocks[j].status == BLOCK_ERROR_KNOWN_POS) {
                    mzd_add(seg, seg, cfg->blocks[j].syndrome);
                }
                if (!b) {
                    b = seg;
                } else {
                    mzd_t *tmp = mzd_stack(NULL, b, seg);
                    mzd_free(b);
                    mzd_free(seg);
                    b = tmp;
                }
            }
            assert(b);

            // check solvability
            bool solvable = solver_check(ctx, b);
            mzd_free(b);
            if (solvable) {
                // cleanup and return true
                solver_free(ctx);
                mzd_free(A_large);
                for (int k = 0; k < NUM_BLOCKS; ++k) {
                    mzd_free(A_list[k]);
                    mzd_free(b_base[k]);
                }
                return true;
            }
        }

        // cleanup per‐unknown
        solver_free(ctx);
        mzd_free(A_large);
    }

    // 5) cleanup and return false
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        mzd_free(A_list[i]);
        mzd_free(b_base[i]);
    }
    return false;
}


static void print_progress(size_t current, size_t total) {
    const int BAR_WIDTH = 50;
    float fraction = (float)current / (float)total;
    int filled = (int)(fraction * BAR_WIDTH);

    printf("\r[");
    for (int i = 0; i < BAR_WIDTH; ++i) {
        putchar(i < filled ? '=' : ' ');
    }
    printf("] %3zu/%3zu", current, total);
    fflush(stdout);
}

int main(void) {
    // 1) Generate and populate all candidate error configurations once
    error_config_list_t configs;
    generate_error_configs(&configs);
    populate_error_config_syndromes(&configs);
    init_globals();

    // 2) Initialize globals once (no R4 yet)
    // We want lazy per‐R4 init inside is_valid_r4, so here nothing

    // 3) Iterate over all R4 indices and test validity with progress bar
    printf("Valid R4 candidates:\n");
    size_t total = (size_t)R4_SPACE;
    for (size_t r4 = 0; r4 < total; ++r4) {
        print_progress(r4, total);
        if (is_invalid_r4((uint16_t)r4, &configs)) {
            printf("\n  ❌ R4 = %zu\n", r4);
       
           continue; // skip invalid R4s
        }
        if (is_valid_r4((uint16_t)r4, &configs)) {
            printf("\n  ✅ R4 = %zu\n", r4);
        }
    }
    // finish bar
    print_progress(total, total);
    printf("\nDone.\n");

    // 4) Cleanup
    free(configs.list);
    return 0;
}
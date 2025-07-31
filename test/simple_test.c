// File: test/run_error_configs.c

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "m4ri/m4ri.h"
#include "decrypt.h"            // init_globals_for_r4
#include "error_bits.h"         // generate_error_configs, populate_error_config_syndromes

/**
 * @brief   For a given R4 index, iterate through all generated error configurations,
 *          test solvability, and write results to a CSV file.
 *
 * @param   R4         the R4 index to process (0 … R4_SPACE-1)
 * @param   out_path   path to the output CSV file
 * @return             true on success, false on any failure
 */
bool process_and_save_solutions(uint16_t R4, const char *out_path) {
    // 1) fast‐init only R4
    init_globals_for_r4(R4);

    // 2) generate & syndrome‐populate configs
    error_config_list_t configs;
    generate_error_configs(&configs);
    populate_error_config_syndromes(&configs);

    // 3) build per‐block system once
    mzd_t *A_list[NUM_BLOCKS], *b_base[NUM_BLOCKS];
    assemble_system(R4, A_list, b_base);

    // 4) open output file
    FILE *f = fopen(out_path, "w");
    if (!f){
    printf("Error opening output file: %s\n", out_path);
        for (int i = 0; i < NUM_BLOCKS; ++i) {
            mzd_free(A_list[i]);
            mzd_free(b_base[i]);
        }
        free(configs.list);
    return false;
    }
    // write CSV header
    fprintf(f, "config_index,unknown_block,solvable\n");

    // 5) segment size
    size_t segment = 1 + (NUM_BLOCKS - 1) * CIPHERTEXT_SIZE;

    // 6) for each unknown block
    for (int unknown = 0; unknown < NUM_BLOCKS; ++unknown) {
        // assemble large A and prepare solver
        mzd_t *A_large = NULL;
        assemble_A_for_unknown(A_list, unknown, &A_large);
        solver_ctx_t *ctx = solver_prepare(A_large);

        // test each config in this unknown’s segment
        size_t start = unknown * segment;
        size_t end   = start + segment;
        for (size_t idx = start; idx < end; ++idx) {
            error_bits_t *cfg = &configs.list[idx];

            // build b by stacking per-block segments
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
            fprintf(f, "%zu,%d,%d\n", idx, unknown, solvable ? 1 : 0);

            mzd_free(b);
        }

        solver_free(ctx);
        mzd_free(A_large);
    }

    // 7) cleanup
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        mzd_free(A_list[i]);
        mzd_free(b_base[i]);
    }
    free(configs.list);

    fclose(f);
    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <R4_index> <output_csv>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse R4 index
    char *endptr;
    long r4 = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || r4 < 0 || r4 >= R4_SPACE) {
        fprintf(stderr, "Invalid R4 index: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    const char *out_path = argv[2];

    // Process and save
    if (!process_and_save_solutions((uint16_t)r4, out_path)) {
        fprintf(stderr, "Error: failed to process R4=%ld\n", r4);
        return EXIT_FAILURE;
    }

    printf("Results for R4=%ld written to %s\n", r4, out_path);
    return EXIT_SUCCESS;
}
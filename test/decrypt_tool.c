// File: test/decrypt_tool.c

#include <stdio.h>
#include <stdlib.h>
#include <m4ri/m4ri.h>
#include "lfsr_state.h"    // lfsr_matrices_init, lfsr_matrix_initialization, lfsr_matrices_cleanup
#include "encrypt.h"       // extract_variables_from_state, expand_states_linearized_m4ri
#include "decrypt.h"       // init_v_diff_matrices, free_v_diff_matrices, V_DIFF_MATS

int main(void) {
    // 1) Initialize companion matrices and zS
    lfsr_matrices_init();

    // 2) Build V_DIFF_MATS
    init_v_diff_matrices();
    printf("V_DIFF_MATS initialized for %d blocks.\n", V_DIFF_COUNT);

// 3) Prepare state0 and v0
    uint32_t R1_init = (rand() & ((1u << 19) - 1)) | 1u;
    uint32_t R2_init = (rand() & ((1u << 22) - 1)) | 1u;
    uint32_t R3_init = (rand() & ((1u << 23) - 1)) | 1u;
    uint32_t R4_init = (rand() & ((1u << 17) - 1)) | 1u;
    uint32_t S0[4]  = { R1_init, R2_init, R3_init, R4_init };

    // 2) 두 state 구조체에 동일하게 초기화
    lfsr_matrix_state_t state0 = {0};
    printf(state0.v == NULL ? "state0.v is NULL\n" : "state0.v is initialized\n");
    lfsr_matrix_initialization_regs(&state0, R1_init, R2_init, R3_init, R4_init);
        printf(state0.v == NULL ? "state0.v is NULL\n" : "state0.v is initialized\n");
    extract_variables_from_state(&state0);  // fills state0.v

    // 4) Expand via zS-based linearization
    lfsr_matrix_state_t* S_zs[NUM_BLOCKS]={0};
    expand_states_linearized_m4ri(&state0, NUM_BLOCKS, S_zs);
    printf("Expanded %d states from initial state.\n", NUM_BLOCKS);
    // Extract v_zs[i] pointers
    mzd_t* v_zs[NUM_BLOCKS];

    for (int i = 0; i < NUM_BLOCKS; i++) {
        extract_variables_from_state(S_zs[i]);
        v_zs[i] = S_zs[i]->v;
    }

    // 5) Compare for each block
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // Method 1: v_zs[i]
        mzd_t* vz = v_zs[i];

        // Method 2: v_vdiff = v0 * transform
        mzd_t* vvd = mzd_init(1, TOTAL_VARS);
        if (i == 0) {
            // block 0: identity
            mzd_copy(vvd, state0.v);
        } else {
            // block i>0: use V_DIFF_MATS[i-1]
            mzd_mul(vvd, state0.v, V_DIFF_MATS[i-1],0);
        }

        // Compare bits
        int ok = 1;
        for (int j = 0; j < TOTAL_VARS; j++) {
            int b1 = mzd_read_bit(vz,   0, j);
            int b2 = mzd_read_bit(vvd,  0, j);
            if (b1 != b2) { ok = 0; break; }
        }
        printf("Block %2d v match: %s\n", i, ok ? "OK" : "MISMATCH");

        mzd_free(vvd);
    }

    // 6) Cleanup
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // free only the extra structs, not the v inside state0
        mzd_free(S_zs[i]->R1);
        mzd_free(S_zs[i]->R2);
        mzd_free(S_zs[i]->R3);
        mzd_free(S_zs[i]->R4);
        mzd_free(S_zs[i]->v);
        free(S_zs[i]);
    }
    free_v_diff_matrices();
    lfsr_matrices_cleanup();
    mzd_free(state0.v);
    mzd_free(state0.R1);
    mzd_free(state0.R2);
    mzd_free(state0.R3);
    mzd_free(state0.R4);

    return 0;
}

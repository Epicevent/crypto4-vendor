// Compile with:
//   mkdir -p bin
//   gcc -O2 crypto4_test.c -o bin/crypto4_test \
//       -Iinclude -I3rdparty/m4ri/build/include \
//       -L3rdparty/m4ri/build/lib -lm4ri -lm

#include <stdio.h>
#include <stdlib.h>
#include <m4ri/m4ri.h>
#include "crypto_lib.h"
#include "encrypt.h"

#define NUM_TEST_STATES 5

int main(void) {
    printf("Crypto4 Library Extended Test\n");
    printf("============================\n\n");

    // 1. Initialize m4ri and Crypto4
    printf("1. Initializing m4ri and Crypto4...\n");
    m4ri_init();
    lfsr_matrices_init();
    printf("   ✓ Initialization complete\n\n");

    // 2. Basic state expansion test
    printf("2. Testing state expansion...\n");
    lfsr_matrix_state_t S0;
    S0.R1 = mzd_init(1, 19);
    S0.R2 = mzd_init(1, 22);
    S0.R3 = mzd_init(1, 23);
    S0.R4 = mzd_init(1, 17);
    // set LSB = 1
    mzd_write_bit(S0.R1, 0, 0, 1);
    mzd_write_bit(S0.R2, 0, 0, 1);
    mzd_write_bit(S0.R3, 0, 0, 1);
    mzd_write_bit(S0.R4, 0, 0, 1);
    lfsr_matrix_state_t* states[NUM_TEST_STATES];
    expand_states_from_initial_m4ri(&S0, NUM_TEST_STATES, states);
    printf("   ✓ Expanded %d states successfully\n\n", NUM_TEST_STATES);

    // 3. Test loading Gt.bin and deriving G
    printf("3. Testing Gt.bin load and G derivation...\n");
    FILE *f_gt = fopen("data/Gt.bin", "rb");
    if (!f_gt) { perror("fopen Gt.bin"); return 1; }
    mzd_t *Gt = mzd_read_bin(f_gt);
    fclose(f_gt);
    printf("   - Gt dimensions: %zu x %zu\n", mzd_nrows(Gt), mzd_ncols(Gt));
    // transpose Gt to get G (208x160)
    mzd_t *G = mzd_transpose(NULL, Gt);
    mzd_free(Gt);

    // 4. Test loading H.bin
    printf("4. Testing H.bin load...\n");
    FILE *f_h = fopen("data/H.bin", "rb");
    if (!f_h) { perror("fopen H.bin"); return 1; }
    mzd_t *H = mzd_read_bin(f_h);
    fclose(f_h);
    printf("   - H dimensions: %zu x %zu\n", mzd_nrows(H), mzd_ncols(H));

    // 5. Verify H * G = 0
    printf("5. Verifying H * G = 0...\n");
    mzd_t *HG = mzd_mul(NULL, H, G);
    if (mzd_is_zero(HG)) {
        printf("   ✓ H*G is zero matrix\n\n");
    } else {
        fprintf(stderr, "   ✗ H*G is not zero!\n");
        mzd_print(HG);
        return 1;
    }

    // 6. Cleanup
    printf("6. Cleaning up...\n");
    for (int i = 0; i < NUM_TEST_STATES; i++) {
        mzd_free(states[i]->R1);
        mzd_free(states[i]->R2);
        mzd_free(states[i]->R3);
        mzd_free(states[i]->R4);
        free(states[i]);
    }
    mzd_free(S0.R1);
    mzd_free(S0.R2);
    mzd_free(S0.R3);
    mzd_free(S0.R4);
    mzd_free(G);
    mzd_free(H);
    mzd_free(HG);

    lfsr_matrices_cleanup();
    printf("\n✅ All extended tests passed!\n");
    return 0;
}

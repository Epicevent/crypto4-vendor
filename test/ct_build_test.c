#include <stdio.h>
#include <time.h>
#include <m4ri/m4ri.h>
#include <stdint.h>
#include <stdbool.h>
#include "encrypt.h"
#include "decrypt.h"

#define R4_SPACE   (1<<16)
#define C_ROWS     208
#define TOTAL_VARS 656

static mzd_t *Ct_cache[R4_SPACE] = { NULL };
static bool cache_inited = false;

static mzd_t *get_Ct_for_r4(uint32_t r4_index) {
    if (!cache_inited) {
        for (int i = 0; i < R4_SPACE; i++) Ct_cache[i] = NULL;
        cache_inited = true;
    }
    mzd_t *Ct = Ct_cache[r4_index];
    if (!Ct) {
        mzd_t *C = mzd_init(C_ROWS, TOTAL_VARS);
        build_linear_system_with_pattern(
            get_clock_pattern((r4_index<<1)|1),
            C
        );
        Ct = mzd_init(TOTAL_VARS, C_ROWS);
        mzd_transpose(Ct, C);
        mzd_free(C);
        Ct_cache[r4_index] = Ct;
    }
    return Ct;
}

static void free_Ct_cache(void) {
    for (int i = 0; i < R4_SPACE; i++) {
        if (Ct_cache[i]) {
            mzd_free(Ct_cache[i]);
            Ct_cache[i] = NULL;
        }
    }
}

int main(void) {
    clock_t t0, t1;
    init_clock_patterns();
    lfsr_matrices_init();

    printf("Building %u Ct matrices: 0%%", R4_SPACE);
    fflush(stdout);

    t0 = clock();
    for (uint32_t r4 = 0; r4 < R4_SPACE; r4++) {
        get_Ct_for_r4(r4);
        // 1 000단위로 진행률 업데이트
        if ((r4 & 0x3FF) == 0) {
            int pct = (int)(100.0 * r4 / R4_SPACE);
            printf("\rBuilding %u Ct matrices: %3d%%", R4_SPACE, pct);
            fflush(stdout);
        }
    }
    t1 = clock();

    // 완료 표시
    printf("\rBuilding %u Ct matrices: 100%%\n", R4_SPACE);

    double secs = (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("Done in %.3f seconds\n", secs);

    free_Ct_cache();
    return 0;
}

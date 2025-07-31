// test_solvability_speed.c

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "m4ri/m4ri.h"
#include "decrypt.h"  

static double diff_secs(clock_t end, clock_t start) {
    return (double)(end - start) / CLOCKS_PER_SEC;
}

int main(void) {
    srand((unsigned)time(NULL));

    const int m = 672, n = 656;
    const int trials = 1000;

    double total_inc = 0.0;
    double total_direct = 0.0;

    for (int t = 0; t < trials; ++t) {
        // 1) 랜덤 tall matrix A (m×n)
        mzd_t *A = mzd_init(m, n);
        for (int i = 0; i < m; ++i)
            for (int j = 0; j < n; ++j)
                mzd_write_bit(A, i, j, rand() & 1);

        // 2) rank(A) 계산 once (direct only)
        mzd_t *A_copy = mzd_copy(NULL, A);
        rci_t rankA = mzd_gauss_delayed(A_copy, 0, FALSE);
        mzd_free(A_copy);

        // 3) 랜덤 b 생성 (m×1)
        mzd_t *b = mzd_init(m, 1);
        for (int i = 0; i < m; ++i)
            mzd_write_bit(b, i, 0, rand() & 1);

        // 4) 증분 검사 timing
        clock_t t0 = clock();
        bool inc = check_solvability_incremental(A, b);
        clock_t t1 = clock();
        total_inc += diff_secs(t1, t0);

        // 5) 직접 rank 검사 timing
        //    build [A|b] via stack+transpose for fairness
        mzd_t *AT   = mzd_transpose(NULL, A);
        mzd_t *bT   = mzd_transpose(NULL, b);
        mzd_t *T    = mzd_stack(NULL, AT, bT);
        mzd_t *Aug  = mzd_transpose(NULL, T);
        mzd_free(AT);
        mzd_free(bT);
        mzd_free(T);

        clock_t t2 = clock();
        rci_t rankAug = mzd_gauss_delayed(Aug, 0, FALSE);
        clock_t t3 = clock();
        total_direct += diff_secs(t3, t2);

        bool direct = (rankAug == rankA);
        mzd_free(Aug);

        if (inc != direct) {
            fprintf(stderr, "Mismatch on trial %d\n", t);
            abort();
        }

        mzd_free(A);
        mzd_free(b);
    }

    printf("Average incremental check time: %.6f s\n", total_inc / trials);
    printf("Average direct   check time: %.6f s\n", total_direct / trials);
    return 0;
}

// Compile with:
//   mkdir -p bin
//   gcc -O2 tools/gen_H_bin.c -o bin/gen_H_bin \
//       -Iinclude -I3rdparty/m4ri/build/include \
//       -L3rdparty/m4ri/build/lib -lm4ri -lm

#include <stdio.h>
#include <stdlib.h>
#include <m4ri/m4ri.h>

#define Gt_FILE   "data/Gt.bin"
#define H_OUTFILE "data/H.bin"

int main(void) {
    // 1) Initialize m4ri
    m4ri_init();

    // 2) Load Gᵀ from file
    FILE *f_gt = fopen(Gt_FILE, "rb");
    if (!f_gt) {
        perror("fopen Gt.bin");
        return 1;
    }
    mzd_t *Gt = mzd_read_bin(f_gt);
    fclose(f_gt);

    // 3) Compute left null-space of G: H * G = 0
    //    nullspace(Gt) gives rows spanning nullspace of Gt (i.e. left nullspace of G)
    mzd_t *H = mzd_nullspace(Gt);

    // 4) Optionally row-reduce for consistency
    mzd_echelon(H, 0);

    // 5) Write H to binary file
    FILE *f_h = fopen(H_OUTFILE, "wb");
    if (!f_h) {
        perror("fopen H.bin");
        mzd_free(Gt);
        mzd_free(H);
        return 1;
    }
    mzd_write_bin(f_h, H);
    fclose(f_h);

    // 6) Clean up
    mzd_free(Gt);
    mzd_free(H);

    printf("H matrix successfully generated and saved to %s\n", H_OUTFILE);
    return 0;
}

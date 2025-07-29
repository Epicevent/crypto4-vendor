#include <stdio.h>
#include <stdlib.h>
#include <m4ri/m4ri.h>

static unsigned char* load_packed_bin(const char *path, size_t *out_bytes) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t n = ftell(f);
    rewind(f);
    unsigned char *buf = malloc(n);
    fread(buf,1,n,f);
    fclose(f);
    *out_bytes = n;
    return buf;
}
/**
 * @brief  mzd_write_bit 만 써서 n×n Identity 행렬 생성
 */
static inline mzd_t *mzd_identity_bit(int n) {
    mzd_t *I = mzd_init(n, n);
    if (!I) return NULL;
    for (int i = 0; i < n; i++) {
        mzd_write_bit(I, i, i, 1);
    }
    return I;
}
int verify_identity(mzd_t *I) {
    int n = I->ncols;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int bit = mzd_read_bit(I, i, j);
            if ((i == j && bit != 1) || (i != j && bit != 0)) {
                // 잘못된 성분 발견!
                printf("Mismatch at (%d,%d): expected %d, got %d\n",
                       i, j, (i == j), bit);
                return 0;
            }
        }
    }
    return 1;
}

int main(void) {
    m4ri_init();
    // 1) load Gt.bin (208×160 bits => 4160 bytes)
    size_t Gt_bytes;
    unsigned char *Gt_buf = load_packed_bin("data/Gt.bin", &Gt_bytes);
    mzd_t *G = mzd_init(208, 160);
    // unpack Gt_buf into G
    // …

    // 2) load H.bin (48×208 bits => 1248 bytes)
    size_t H_bytes;
    unsigned char *H_buf = load_packed_bin("data/H.bin", &H_bytes);
    mzd_t *H = mzd_init(48, 208);
    // unpack H_buf into H
    // …

    // 3) compute HG
    mzd_t *HG = mzd_init(48, 160);
    mzd_mul(HG, H, G, 0);

    // 4) verify zero
    if (!mzd_is_zero(HG)) {
        printf("H*G != 0\n");
        return 1;
    }
    printf("H*G == 0\n");

    // cleanup
    mzd_free(G); mzd_free(H); mzd_free(HG);
    free(Gt_buf); free(H_buf);
    int n = 19;
    mzd_t *I = mzd_identity_bit(n);
    if (!I) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    printf("Verifying 19×19 identity… ");
    if (verify_identity(I)) {
        printf("OK, all non‑diagonal bits are 0 and diagonals are 1.\n");
    } else {
        printf("ERROR found!\n");
    }

    mzd_free(I);
}
    
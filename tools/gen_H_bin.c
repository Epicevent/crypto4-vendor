// tools/gen_H_bin.c
#include <stdio.h>
#include <stdlib.h>
#include <m4ri/m4ri.h>

#define ROWS_G 208
#define COLS_G 160

// Packs an mzd_t (rows×cols) into a .bin (8 bits/byte MSB-first)
static int pack_and_write(const char *path, mzd_t *M) {
    int rows = M->nrows, cols = M->ncols;
    int nbits = rows * cols, nbytes = nbits / 8;
    unsigned char *buf = calloc(nbytes, 1);
    if (!buf) return 0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (mzd_read_bit(M, i, j)) {
                int idx = i*cols + j;
                buf[idx>>3] |= 1 << (7 - (idx & 7));
            }
        }
    }
    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return 0; }
    fwrite(buf, 1, nbytes, f);
    fclose(f);
    free(buf);
    return 1;
}

int main(void) {
    m4ri_init();

    // 1) Read raw Gt.bin into a fresh mzd_t* G
    size_t nbits = ROWS_G * COLS_G, nbytes = nbits/8;
    unsigned char *buf = malloc(nbytes);
    FILE *f = fopen("data/Gt.bin","rb");
    if (!f || fread(buf,1,nbytes,f)!=nbytes) {
        fprintf(stderr,"ERROR reading data/Gt.bin\n"); return 1;
    }
    fclose(f);

    mzd_t *G = mzd_init(ROWS_G, COLS_G);
    for (int i=0;i<ROWS_G;i++) for(int j=0;j<COLS_G;j++){
        size_t idx = i*COLS_G + j;
        if ((buf[idx>>3] >> (7-(idx&7))) & 1)
            mzd_write_bit(G,i,j,1);
    }
    free(buf);
   

    // 2) Transpose → GT
    mzd_t *GT = mzd_transpose(NULL, G);
    mzd_free(G);
    if (!GT) { fprintf(stderr,"ERROR transpose\n"); return 1; }


    // 3) Echelonize
    int rank = mzd_echelonize(GT,0);
    if(rank<0){fprintf(stderr,"ERROR echelonize\n"); return 1;}
    int nullity = GT->ncols - rank;


    // 4) Extract pivots
    int *pivots = malloc(rank * sizeof(int));
    for (int r = 0, c = 0; c < GT->ncols && r < rank; c++) {
        if (mzd_read_bit(GT, r, c)) {
            pivots[r++] = c;
        }
    }

    // 5) Build null-space matrix H
    mzd_t *H = mzd_init(nullity, GT->ncols);
    for (int fc = 0, row = 0; fc < GT->ncols; fc++) {
        // skip pivots
        int is_p = 0;
        for (int i = 0; i < rank; i++)
            if (pivots[i] == fc) { is_p = 1; break; }
        if (is_p) continue;

        mzd_write_bit(H, row, fc, 1);
        for (int i = rank-1; i >= 0; i--) {
            int p = pivots[i], sum = 0;
            for (int j = p+1; j < GT->ncols; j++)
                sum ^= mzd_read_bit(GT, i, j) & mzd_read_bit(H, row, j);
            mzd_write_bit(H, row, p, sum);
        }
        row++;
    }

    // 6) Write H.bin

    if (!pack_and_write("data/H.bin", H)) {
        mzd_free(GT);
        mzd_free(H);
        free(pivots);
        return 1;
    }
    // cleanup
    mzd_free(GT);
    mzd_free(H);
    free(pivots);
    return 0;
}

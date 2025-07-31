# include "lfsr_state.h"
# include <m4ri/m4ri.h>
# include <stdio.h>
// Paste into lfsr_state.c:

// Companion‑matrix cache
mzd_t *A1 = NULL;  // R1 companion matrix (19×19)
mzd_t *A2 = NULL;  // R2 companion matrix (22×22)
mzd_t *A3 = NULL;  // R3 companion matrix (23×23)
mzd_t *A4 = NULL;  // R4 companion matrix (17×17)

// zS‑matrix cache
mzd_t *zS_R1 = NULL;  // zS R1 matrix (ZS_ROWS×19)
mzd_t *zS_R2 = NULL;  // zS R2 matrix (ZS_ROWS×22)
mzd_t *zS_R3 = NULL;  // zS R3 matrix (ZS_ROWS×23)
mzd_t *zS_R4 = NULL;  // zS R4 matrix (ZS_ROWS×17)

// Clock patterns lookup
uint8_t *clock_patterns = NULL;

void verify_companion_matrices(void) {
    struct spec { mzd_t *mat; int rows, cols; const char *name; };
    struct spec specs[] = {
        { A1, 19, 19, "A1 (R1)" },
        { A2, 22, 22, "A2 (R2)" },
        { A3, 23, 23, "A3 (R3)" },
        { A4, 17, 17, "A4 (R4)" },
    };
    int all_ok = 1;
    for (int i = 0; i < 4; ++i) {
        mzd_t *mat        = specs[i].mat;
        int     exp_rows  = specs[i].rows;
        int     exp_cols  = specs[i].cols;
        const char *name  = specs[i].name;

        if (!mat) {
            fprintf(stderr, "✘ %s is NULL\n", name);
            all_ok = 0;
        } else if (mat->nrows != exp_rows || mat->ncols != exp_cols) {
            fprintf(stderr,
                    "✘ %s has wrong size: got %dx%d, expected %dx%d\n",
                    name,
                    mat->nrows, mat->ncols,
                    exp_rows, exp_cols);
            all_ok = 0;
        } else {
            printf("✔ %s OK (%dx%d)\n", name, mat->nrows, mat->ncols);
        }
    }
    if (!all_ok) {
        fprintf(stderr, "Companion matrix verification FAILED\n");
        abort();
    }
}

void init_clock_patterns(void) {
    if (clock_patterns) return;
    clock_patterns = malloc((size_t)CLOCK_PATTERN_STATES * CLOCK_PATTERN_LEN);
    if (!clock_patterns) abort();
    FILE *f = fopen(CLOCK_PATTERNS_FILE, "rb");
    if (!f) abort();
    size_t got = fread(clock_patterns, 1,
                       (size_t)CLOCK_PATTERN_STATES * CLOCK_PATTERN_LEN,
                       f);
    fclose(f);
    if (got != (size_t)CLOCK_PATTERN_STATES * CLOCK_PATTERN_LEN) abort();
}

const uint8_t* get_clock_pattern(uint16_t r4_index) {
    if (!clock_patterns) abort();            // 반드시 init 먼저
    if (r4_index >= CLOCK_PATTERN_STATES) abort();
    return clock_patterns + (size_t)r4_index * CLOCK_PATTERN_LEN;
}

void cleanup_clock_patterns(void) {
    free(clock_patterns);
    clock_patterns = NULL;
}

// LFSR 상태 초기화: lfsr_matrix_state_t* state
void lfsr_matrix_initialization(lfsr_matrix_state_t* state) {
    if (!state) return;
    // 기존 메모리 해제 및 NULL 대입
    if (state->R1) { mzd_free(state->R1); state->R1 = NULL; }
    if (state->R2) { mzd_free(state->R2); state->R2 = NULL; }
    if (state->R3) { mzd_free(state->R3); state->R3 = NULL; }
    if (state->R4) { mzd_free(state->R4); state->R4 = NULL; }
    if (state->v) { mzd_free(state->v); state->v = NULL; }

    // R1: len=19, fp=0xE4000, ff=0x7FFFF
    // R2: len=22, fp=0x622000, ff=0x3FFFFF
    // R3: len=23, fp=0xCC0000, ff=0x7FFFFF
    // R4: len=17, fp=0x26200, ff=0x1FFFF
    state->R1 = mzd_init(1, 19);
    state->R2 = mzd_init(1, 22);
    state->R3 = mzd_init(1, 23);
    state->R4 = mzd_init(1, 17);
    for (int i = 0; i < 19; i++) mzd_write_bit(state->R1, 0, i, 0);
    for (int i = 0; i < 22; i++) mzd_write_bit(state->R2, 0, i, 0);
    for (int i = 0; i < 23; i++) mzd_write_bit(state->R3, 0, i, 0);
    for (int i = 0; i < 17; i++) mzd_write_bit(state->R4, 0, i, 0);
}
void lfsr_matrix_initialization_regs(
    lfsr_matrix_state_t *state,
    uint32_t             R1_init,
    uint32_t             R2_init,
    uint32_t             R3_init,
    uint32_t             R4_init
) {
    if (!state) return;
    // 기존 메모리 해제 및 NULL 대입
    if (state->R1) { mzd_free(state->R1); state->R1 = NULL; }
    if (state->R2) { mzd_free(state->R2); state->R2 = NULL; }
    if (state->R3) { mzd_free(state->R3); state->R3 = NULL; }
    if (state->R4) { mzd_free(state->R4); state->R4 = NULL; }
    if (state->v) { mzd_free(state->v); state->v = NULL; }
    // allocate matrices
    state->R1 = mzd_init(1, 19);
    state->R2 = mzd_init(1, 22);
    state->R3 = mzd_init(1, 23);
    state->R4 = mzd_init(1, 17);
    state->v = mzd_init(1, TOTAL_VARS); // 1x656 변수 벡터: R1, R2, R3의 비트 및 비트 조합 정보 저장
    // write bits from the provided init words
    for (int i = 0; i < 19; ++i)
        mzd_write_bit(state->R1, 0, i, (R1_init >> i) & 1);
    for (int i = 0; i < 22; ++i)
        mzd_write_bit(state->R2, 0, i, (R2_init >> i) & 1);
    for (int i = 0; i < 23; ++i)
        mzd_write_bit(state->R3, 0, i, (R3_init >> i) & 1);
    for (int i = 0; i < 17; ++i)
        mzd_write_bit(state->R4, 0, i, (R4_init >> i) & 1);

}
void extract_variables_from_state(lfsr_matrix_state_t* state) {
    // 1×656 벡터로 확장: 상수항(1) + 기존 655 vars'
    if (!state->v) {
        state->v = mzd_init(1, TOTAL_VARS);
    }
    // 전체를 0으로 초기화
// ↓ 이렇게 전부 직접 0으로 초기화하세요:
    for (int j = 0; j < TOTAL_VARS; ++j) {
        mzd_write_bit(state->v, 0, j, 0);
    }
    // 상수항: v[0] = 1
    assert(lfsr_matrix_get(state->R1, 0) == 1);
    assert(lfsr_matrix_get(state->R2, 0) == 1);
    assert(lfsr_matrix_get(state->R3, 0) == 1);
    assert(lfsr_matrix_get(state->R4, 0) == 1);
    mzd_write_bit(state->v, 0, 0, 1);
    int idx = 1;  // 이제부터 실제 변수들이 채워질 위치
    
    // --- R1: 길이 19, X1..X18 단일항 ---
    for (int i = 1; i < 19; ++i) {
        int xi = lfsr_matrix_get(state->R1, i);
        mzd_write_bit(state->v, 0, idx++, xi);
        
    }
    // R1: 2-비트 조합
    for (int i = 1; i < 19; ++i) {
        int xi = lfsr_matrix_get(state->R1, i);
        for (int j = i + 1; j < 19; ++j) {
            int xj = lfsr_matrix_get(state->R1, j);
            mzd_write_bit(state->v, 0, idx++, xi & xj);
            
        }
    }

    // --- R2: 길이 22, X1..X21 단일항 ---
    for (int i = 1; i < 22; ++i) {
        int xi = lfsr_matrix_get(state->R2, i);
        mzd_write_bit(state->v, 0, idx++, xi);
       
    }
    // R2: 2-비트 조합
    for (int i = 1; i < 22; ++i) {
        int xi = lfsr_matrix_get(state->R2, i);
        for (int j = i + 1; j < 22; ++j) {
            int xj = lfsr_matrix_get(state->R2, j);
            mzd_write_bit(state->v, 0, idx++, xi & xj);
            
        }
    }

    // --- R3: 길이 23, X1..X22 단일항 ---
    for (int i = 1; i < 23; ++i) {
        int xi = lfsr_matrix_get(state->R3, i);
        mzd_write_bit(state->v, 0, idx++, xi);
       
    }
    // R3: 2-비트 조합
    for (int i = 1; i < 23; ++i) {
        int xi = lfsr_matrix_get(state->R3, i);
        for (int j = i + 1; j < 23; ++j) {
            int xj = lfsr_matrix_get(state->R3, j);
            mzd_write_bit(state->v, 0, idx++, xi & xj);
           
        }
    }

    // 검증: 1 상수항 + 654 단일항 + C(18,2)+C(21,2)+C(22,2) = 656
    assert(idx == TOTAL_VARS);
}



// --- 전역 행렬 초기화 함수 ---
void lfsr_matrices_init(void) {
    // A 행렬들 초기화
    if (!A1) A1 = lfsr_companion_matrix_transposed(0xE4000, 19);
    if (!A2) A2 = lfsr_companion_matrix_transposed(0x622000, 22);
    if (!A3) A3 = lfsr_companion_matrix_transposed(0xCC0000, 23);
    if (!A4) A4 = lfsr_companion_matrix_transposed(0x26200, 17);
    
    // zS 행렬들 초기화 (한 번만)
    if (!zS_R1) {
        FILE* fzS = fopen("data/zS.bin", "rb");
        if (!fzS) {
            fprintf(stderr, "[m4ri] Failed to open data/zS.bin\n");
            abort();
        }
        
        // zS 행렬들 할당
        zS_R1 = mzd_init(ZS_ROWS, 19);
        zS_R2 = mzd_init(ZS_ROWS, 22);
        zS_R3 = mzd_init(ZS_ROWS, 23);
        zS_R4 = mzd_init(ZS_ROWS, 17);
        
        // zS.bin에서 비트 읽어서 행렬로 설정
        for (int i = 0; i < ZS_ROWS; i++) {
            // R1 (18 bits, excluding LSB)
            for (int j = 1; j < 19; j++) {
                int bit = fgetc(fzS);
                mzd_write_bit(zS_R1, i, j, bit);
            }
            // R2 (21 bits, excluding LSB)
            for (int j = 1; j < 22; j++) {
                int bit = fgetc(fzS);
                mzd_write_bit(zS_R2, i, j, bit);
            }
            // R3 (22 bits, excluding LSB)
            for (int j = 1; j < 23; j++) {
                int bit = fgetc(fzS);
                mzd_write_bit(zS_R3, i, j, bit);
            }
            // R4 (16 bits, excluding LSB)
            for (int j = 1; j < 17; j++) {
                int bit = fgetc(fzS);
                mzd_write_bit(zS_R4, i, j, bit);
            }
            
            // LSB는 0으로 설정 (나중에 정규화)
            mzd_write_bit(zS_R1, i, 0, 0);
            mzd_write_bit(zS_R2, i, 0, 0);
            mzd_write_bit(zS_R3, i, 0, 0);
            mzd_write_bit(zS_R4, i, 0, 0);
        }
        fclose(fzS);
        printf("[m4ri] LFSR companion matrices and zS matrices initialized\n");
    }
}

void lfsr_matrices_cleanup(void) {
    if (A1) { mzd_free(A1); A1 = NULL; }
    if (A2) { mzd_free(A2); A2 = NULL; }
    if (A3) { mzd_free(A3); A3 = NULL; }
    if (A4) { mzd_free(A4); A4 = NULL; }
    
    if (zS_R1) { mzd_free(zS_R1); zS_R1 = NULL; }
    if (zS_R2) { mzd_free(zS_R2); zS_R2 = NULL; }
    if (zS_R3) { mzd_free(zS_R3); zS_R3 = NULL; }
    if (zS_R4) { mzd_free(zS_R4); zS_R4 = NULL; }
    
    printf("[m4ri] LFSR companion matrices and zS matrices cleaned up\n");
}

// --- LFSR companion matrix 생성 ---
mzd_t* lfsr_companion_matrix_transposed(uint32_t fp, int len) {
    mzd_t* A = lfsr_companion_matrix(fp, len);
    mzd_t* AT = mzd_init(len, len);
    for (int i = 0; i < len; i++)
        for (int j = 0; j < len; j++)
            mzd_write_bit(AT, i, j, mzd_read_bit(A, j, i));
    mzd_free(A);
    return AT;
}

// --- LFSR clock: 행렬곱 기반 ---
void lfsr_matrix_clock(mzd_t* lfsr, mzd_t* A) {
    mzd_t* tmp = mzd_init(1, lfsr->ncols);
    mzd_mul(tmp, lfsr, A, 0);
    mzd_copy(lfsr, tmp);
    mzd_free(tmp);
}

int lfsr_matrix_get(const mzd_t* lfsr, int idx) {
    return mzd_read_bit(lfsr, 0, idx);
}

int majority_matrix(int a, int b, int c) {
    return (a & b) ^ (b & c) ^ (c & a);
}

// 기타 유틸리티 함수 등 필요시 추가 

mzd_t* lfsr_companion_matrix(uint32_t fp, int len) {
    mzd_t* A = mzd_init(len, len);
    for (int j = 0; j < len; j++) {
        if (j == len - 1) {
            mzd_write_bit(A, 0, j, 1);
        } else {
            mzd_write_bit(A, 0, j, (fp >> (j + 1)) & 1);
        }
    }
    for (int i = 1; i < len; i++) {
        mzd_write_bit(A, i, i - 1, 1);
    }
    return A;
}

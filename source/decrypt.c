#include "decrypt.h"

//--------------------------------------------------------
// 전역변수 정의
mzd_t *H       = NULL;
mzd_t *Ht      = NULL;
mzd_t *CtHt_cache[R4_SPACE] = { NULL };
mzd_t *c_vecs[NUM_BLOCKS]    = { NULL };
mzd_t *cHt_vecs[NUM_BLOCKS]  = { NULL };
mzd_t *V_DIFF_MATS[ZS_ROWS]  = { NULL };

// 실제 LUT를 채우는 내부 함수
static uint8_t cross3_LUT[8][8];
static bool    cross3_ready = false;
static bool cache_inited = false;

static void get_Ct_for_r4(uint32_t r4_index, mzd_t* Ct) {
    if (Ct == NULL) {
        fprintf(stderr, "get_Ct_for_r4: Ct must be preallocateded\n");
        abort();
    }
        mzd_t *C = mzd_init(C_ROWS, TOTAL_VARS);
        build_linear_system_with_pattern(
            get_clock_pattern(r4_index),
            C
        );
        mzd_transpose(Ct, C);
        mzd_free(C);

}

static void get_C_for_r4(uint32_t r4_index, mzd_t* C) {
    if (C == NULL) {
        fprintf(stderr, "get_C_for_r4: C must be preallocateded\n");
        abort();
    }
    build_linear_system_with_pattern(
    get_clock_pattern(r4_index),
    C
);
}


// init_cHt_vecs / free_cHt_vecs
void init_cHt_vecs(void) {
    if (cHt_vecs[0]) return;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (!c_vecs[i]) {
            fprintf(stderr,"init_cHt_vecs: c_vecs[%d] NULL\n", i);
            abort();
        }

    
        cHt_vecs[i] = mzd_init(1, Ht->ncols); // 1×48


        mzd_mul(cHt_vecs[i], c_vecs[i], Ht, 0);
    }
}
void free_cHt_vecs(void) {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (cHt_vecs[i]) {
            mzd_free(cHt_vecs[i]);
            cHt_vecs[i] = NULL;
        }
    }
}
void init_CtHt_cache(void) {
    if (CtHt_cache[0]) return;  // already done
    printf("Building %u CtHt matrices: 0%%", R4_SPACE);
    fflush(stdout);
    for (uint32_t r4 = 0; r4 < R4_SPACE; r4++) {
        mzd_t *Ct = mzd_init(TOTAL_VARS, C_ROWS);
        get_Ct_for_r4(r4,Ct);           // 656×208
        CtHt_cache[r4] = mzd_init(TOTAL_VARS, Ht->ncols); // 656×48
        mzd_mul(CtHt_cache[r4], Ct, Ht, 0);
        mzd_free(Ct);
                // 1 000단위로 진행률 업데이트
        if ((r4 & 0x3FF) == 0) {
            int pct = (int)(100.0 * r4 / R4_SPACE);
            printf("\rBuilding %u CtHt matrices: %3d%%", R4_SPACE, pct);
            fflush(stdout);
        }
    }
        printf("\rBuilding %u CtHt matrices: 100%%\n", R4_SPACE);
}
void free_CtHt_cache(void) {
    for (uint32_t r4 = 0; r4 < R4_SPACE; r4++) {
        if (CtHt_cache[r4]) {
            mzd_free(CtHt_cache[r4]);
            CtHt_cache[r4] = NULL;
        }
    }
}
void init_globals(void) {
    // 1) init_clock_patterns() 호출
    init_clock_patterns();
    // 2) LFSR companion & zS 행렬 캐시
    
    lfsr_matrices_init();
    // 3) 패리티 행렬 H, Ht
    init_H();
    // 4) Ct × Ht 캐시 (R4_SPACE 개)
    init_CtHt_cache();
    // 5) ciphertext vectors → c_vecs 에 로드된 뒤
    init_c_vecs();  
    // 6) cHt_vecs 초기화
    init_cHt_vecs();
    // 7) v‑difference matrices 
    init_v_diff_matrices();
}
// in decrypt.c



void init_globals_for_r4(uint16_t R4) {
    static bool core_inited = false;
    if (!core_inited) {
        // 공통으로 한번만 해 주어야 할 것들
        // 1) init_clock_patterns() 호출
        init_clock_patterns();
        // 2) LFSR companion & zS 행렬 캐시
        printf("Initializing LFSR matrices\n");
        lfsr_matrices_init();
        // 3) 패리티 행렬 H, Ht
        printf("Initializing H and Ht matrices\n");
        init_H();
        // 4) Ct × Ht 캐시 (R4_SPACE 개)
        printf("Initializing CtHt_cache for R4=%u\n", R4);
        // 5) ciphertext vectors → c_vecs 에 로드된 뒤
        init_c_vecs();  
        // 6) cHt_vecs 초기화
        printf("Initializing cHt_vecs for R4=%u\n", R4);
        init_cHt_vecs();
        // 7) v‑difference matrices 
        printf("Initializing v-difference matrices\n");
        init_v_diff_matrices();
        core_inited = true;
    }

    // R4별로 캐시되는 CtHt_cache[R4] 만 초기화
    // (원래 init_CtHt_cache()가 전부를 순회하던 부분)
    // 여기는 단 하나의 R4에 대해서만 compute
    if (CtHt_cache[R4] == NULL) {
        // CtHt_cache[R4] = Hᵀ·Cᵀ for this R4
        // 먼저 Cᵀ for this R4: (Cᵀ = load or compute from c_vecs & R4)
        mzd_t *Ct = mzd_init(TOTAL_VARS, C_ROWS);
        get_Ct_for_r4(R4,Ct);           // 656×208
        CtHt_cache[R4] = mzd_init(TOTAL_VARS, Ht->ncols); // 656×48
        mzd_mul_naive(CtHt_cache[R4], Ct, Ht);  
        mzd_free(Ct);
    }
}

/**
 * @brief  init_globals()로 할당된 모든 전역 자원을 해제합니다.
 *         프로그램 종료 시 반드시 호출하세요.
 */
void free_globals(void) {
    // 1) v‑difference 해제
    free_v_diff_matrices();
    // 2) c_vecs×Ht 벡터 해제
    free_cHt_vecs();
    // 3) Ct×Ht 캐시 해제
    free_CtHt_cache();
    // 4) H, Ht 해제
    free_H();
    // 5) LFSR 행렬 정리
    lfsr_matrices_cleanup();
    // m4ri_teardown() 없음
}
void init_H(void) {
    if (H) return;
    printf("Loading H matrix from %s\n", CIPHERTEXT_PATH);
    // data/H.bin 은 48×208 비트(1248바이트)여야 합니다.
    size_t bytes;
    unsigned char *buf = load_packed_bin("data/H.bin", &bytes);
    size_t expected = (48 * 208 + 7) / 8;
    if (!buf || bytes != expected) {
        fprintf(stderr, "H.bin load error: got %zu bytes, expected %zu\n",
                bytes, expected);
        abort();
    }

    // H (48×208) 언패킹
    H = mzd_init(48, 208);
    for (int r = 0; r < 48; r++) {
        for (int c = 0; c < 208; c++) {
            size_t idx    = (size_t)r * 208 + c;
            size_t byte_i = idx >> 3;
            int    bit_i  = 7 - (idx & 7);
            int    bit    = (buf[byte_i] >> bit_i) & 1;
            mzd_write_bit(H, r, c, bit);
        }
    }
    free(buf);

    // Ht = H^T (208×48)
    Ht = mzd_init(208, 48);
    if (!Ht) {
        fprintf(stderr, "Failed to allocate Ht\n");
        abort();
    }
    mzd_transpose(Ht, H);
}

void free_H(void) {
    if (Ht) {
        mzd_free(Ht);
        Ht = NULL;
    }
    if (H) {
        mzd_free(H);
        H = NULL;
    }
}


// 실제 LUT를 채우는 내부 함수
static inline uint8_t cross3_calc(int u, int v)
{
    int u0 =  u       & 1;
    int u1 = (u >> 1) & 1;
    int u2 = (u >> 2) & 1;
    int v0 =  v       & 1;
    int v1 = (v >> 1) & 1;
    int v2 = (v >> 2) & 1;

    return (u0 & v1) ^ (u1 & v2) ^ (u2 & v0);
}

static inline int linear_index(const LSegment* seg, int u) {
    // 1) u는 1차항 범위 내(1..reg_len-1)에 있어야 함
    if (!(u > 0 && u < seg->reg_len)) {
        return -1;
    }
    // 2) seg->var_offset 은 이 세그먼트의 첫 1차항 인덱스를 가리킴
    //    u=1이면 var_offset, u=2면 var_offset+1, … 
    return seg->var_offset + (u - 1);
}

// cross3_LUT[u][v] = u0&v1 ^ u1&v2 ^ u2&v0
static void init_cross3_LUT(void)
{
    for (int u = 0; u < 8; ++u) {
        for (int v = 0; v < 8; ++v) {
            cross3_LUT[u][v] = cross3_calc(u, v);
        }
    }
    cross3_ready = true;
}

// 사용 전에 초기화됐는지 확인하고, 아니면 초기화
static inline void ensure_cross3_LUT(void) {
    if (!cross3_ready) {
        init_cross3_LUT();
    }
}

static inline int cross3_lut(int u, int v) {
    ensure_cross3_LUT();
    return cross3_LUT[u][v];
}
static inline int quad_index(const LSegment* seg, int u, int v) {
    int r = seg->reg_len;
    // u<v 가 아니면 2차항이 아님
    if (!(u < v && u > 0 && v < r)) {
        return -1;
    }
    int base = seg->var_offset + r-1;
    int offset = 0;
    // rows 1..u-1 에서 v와 페어가 만들어지는 개수 합산
    for (int i = 1; i < u; ++i) {
        offset += (r - 1 - i);
    }
    // 같은 u행에서 v까지 거리
    offset += (v - u - 1);
    return base + offset;
}



void init_v_diff_matrices(void) {
    if (V_DIFF_MATS[0]) return;     // 이미 초기화됨

    // 1) zS 및 companion matrices 초기화
    lfsr_matrices_init();

    // 2) 각 블럭 i=1..V_DIFF_COUNT에 대해 변환행렬 구축
    for (int i = 1; i <= V_DIFF_COUNT; ++i) {
        int row = i - 1;  // zS 행 인덱스

        // 2.1) 단위행렬 생성 (656×656)
        mzd_t *M = mzd_init(V_DIFF_SIZE, V_DIFF_SIZE);
        // identity 설정
        for (int d = 0; d < V_DIFF_SIZE; ++d) {
            mzd_write_bit(M, d, d, 1);
        }

        // R1 segment
        LSegment seg1;
        init_LSegment(&seg1, VAR_OFF_R1, VAR_LEN_R1, 1);
        for (int j = 1; j < seg1.reg_len; ++j) {
            int li = linear_index(&seg1, j);
            int dj = mzd_read_bit(zS_R1, row, j);
            mzd_write_bit(M, 0, li, dj);
        }
        for (int u = 1; u < seg1.reg_len; ++u) {
            int du = mzd_read_bit(zS_R1, row, u);
            for (int v = u + 1; v < seg1.reg_len; ++v) {
                int dv = mzd_read_bit(zS_R1, row, v);
                int k  = quad_index(&seg1, u, v);
                int lu = linear_index(&seg1, u);
                int lv = linear_index(&seg1, v);
                mzd_write_bit(M, 0, k,    du & dv);
                mzd_write_bit(M, lu, k,   dv);
                mzd_write_bit(M, lv, k,   du);
            }
        }

        // R2 segment
        LSegment seg2;
        init_LSegment(&seg2, VAR_OFF_R2, VAR_LEN_R2, 2);
        for (int j = 1; j < seg2.reg_len; ++j) {
            int li = linear_index(&seg2, j);
            int dj = mzd_read_bit(zS_R2, row, j);
            mzd_write_bit(M, 0, li, dj);
        }
        for (int u = 1; u < seg2.reg_len; ++u) {
            int du = mzd_read_bit(zS_R2, row, u);
            for (int v = u + 1; v < seg2.reg_len; ++v) {
                int dv = mzd_read_bit(zS_R2, row, v);
                int k  = quad_index(&seg2, u, v);
                int lu = linear_index(&seg2, u);
                int lv = linear_index(&seg2, v);
                mzd_write_bit(M, 0, k,    du & dv);
                mzd_write_bit(M, lu, k,   dv);
                mzd_write_bit(M, lv, k,   du);
            }
        }

        // R3 segment
        LSegment seg3;
        init_LSegment(&seg3, VAR_OFF_R3, VAR_LEN_R3, 3);
        for (int j = 1; j < seg3.reg_len; ++j) {
            int li = linear_index(&seg3, j);
            int dj = mzd_read_bit(zS_R3, row, j);
            mzd_write_bit(M, 0, li, dj);
        }
        for (int u = 1; u < seg3.reg_len; ++u) {
            int du = mzd_read_bit(zS_R3, row, u);
            for (int v = u + 1; v < seg3.reg_len; ++v) {
                int dv = mzd_read_bit(zS_R3, row, v);
                int k  = quad_index(&seg3, u, v);
                int lu = linear_index(&seg3, u);
                int lv = linear_index(&seg3, v);
                mzd_write_bit(M, 0, k,    du & dv);
                mzd_write_bit(M, lu, k,   dv);
                mzd_write_bit(M, lv, k,   du);
            }
        }

        // 3) 배열에 저장
        V_DIFF_MATS[i-1] = M;
    }
}

void free_v_diff_matrices(void) {
    for (int i = 0; i < V_DIFF_COUNT; ++i) {
        if (V_DIFF_MATS[i]) {
            mzd_free(V_DIFF_MATS[i]);
            V_DIFF_MATS[i] = NULL;
        }
    }
}




void compute_segment_row(LSegment* seg) {
    int r = seg->reg_len;
    // seg->row 의 첫 행 전체를 0으로 초기화
    mzd_row_clear_offset(seg->row,    // M
                        0,            // row = first (and only) row
                        0);           // coloffset = start at col 0 → row 전체

    // 2) Pull out the 3‐bit taps and the L4 bit for each row into locals
    //check r<32
    if (r > 32) {
        fprintf(stderr, "Error: r (%d) exceeds maximum supported value (32).\n", r);
        exit(EXIT_FAILURE);
    }
    
    word taps[ 32 ];//r<<32
    word l4  [ 32 ];//r<<32
    for (int u = 0; u < r; ++u) {
        word *prow = mzd_row(seg->L, u);  // pointer to L[u][*]
        word  w    = prow[0];             // bits 0..m4ri_radix of that row

        taps[u] =   w        & 0x7;       // low 3 bits = L[u,0..2]
        l4  [u] = (w >> 3) & 0x1;         // bit 3       = L[u,3]
    }

    // 3) Constant term → index 0
    {
        int c0 = cross3_lut(taps[0], taps[0])  // M[0][0]
               ^ l4[0];                        // ⊕ L4[0]
        mzd_write_bit(seg->row, 0, CONSTANT_TERM_INDEX, c0);
    }

    // 4) Linear terms → seg->var_offset + u, for u=1..r-1
    for (int u = 1; u < r; ++u) {
        int c_lin =
            cross3_lut(taps[u], taps[0])       // M[u][0]
          ^ cross3_lut(taps[0], taps[u])       // M[0][u]
          ^ cross3_lut(taps[u], taps[u])       // M[u][u]
          ^ l4[u];                             // ⊕ L4[u]

        mzd_write_bit(seg->row, 0, linear_index(seg,u), c_lin);
    }

    // 5) Quadratic terms → use quad_index for u<v pairs
    for (int u = 1; u < r; ++u) {
        for (int v = u + 1; v < r; ++v) {
            int c_quad =
                cross3_lut(taps[u], taps[v])    // M[u][v]
              ^ cross3_lut(taps[v], taps[u]);   // ⊕ M[v][u]
            int idx = quad_index(seg, u, v);

            mzd_write_bit(seg->row, 0, idx, c_quad);
        }
    }

}

//------------------------------------------------------------------------------
// init_LSegment: initialize E = L^{(-1)}, row vector to zero
//------------------------------------------------------------------------------
void init_LSegment(LSegment* seg,
                   uint16_t var_offset,
                   uint16_t var_len,
                   uint8_t  r
                 )
{
    seg->var_offset = var_offset;
    seg->var_len    = var_len;
    seg->r          = r;


    if (r == 1) {
        seg->reg_len = 19;
        seg->L = mzd_init(19, 4);
        mzd_write_bit(seg->L,  1, 0, 1);
        mzd_write_bit(seg->L,  6, 1, 1);
        mzd_write_bit(seg->L, 15, 2, 1);
        mzd_write_bit(seg->L, 11, 3, 1);
    } else if (r == 2) {
        seg->reg_len = 22; 
        seg->L = mzd_init(22, 4);
        mzd_write_bit(seg->L,  3, 0, 1);
        mzd_write_bit(seg->L,  8, 1, 1);
        mzd_write_bit(seg->L, 14, 2, 1);
        mzd_write_bit(seg->L,  1, 3, 1);
    } else {
        seg->reg_len = 23; 
        seg->L = mzd_init(23, 4);
        mzd_write_bit(seg->L,  4, 0, 1);
        mzd_write_bit(seg->L, 15, 1, 1);
        mzd_write_bit(seg->L, 19, 2, 1);
        mzd_write_bit(seg->L,  0, 3, 1);
    }

    seg->row = mzd_init(1, TOTAL_VARS);
    for (int j = 0; j < TOTAL_VARS; ++j) {
        mzd_write_bit(seg->row, 0, j, 0);
    }
}

//------------------------------------------------------------------------------
// update_LSegment: clock step + accumulate row and d after discard
//------------------------------------------------------------------------------
// update_LSegment: 호출될 때마다 “반드시” A×L 갱신만 수행합니다.
// pattern 읽기, pos 증가는 외부 루프에서 담당하고,
// compute_segment_row 호출도 외부에서 이루어져야 합니다.
void update_LSegment(LSegment* seg) {
    // A 매트릭스 선택
    mzd_t* A   = (seg->r == 1 ? A1 :
                  seg->r == 2 ? A2 : A3);
    // seg->L과 동일 차원 임시 버퍼
    mzd_t* tmp = mzd_init(seg->L->nrows, seg->L->ncols);
    // A × L 계산
    mzd_mul(tmp, A, seg->L, 0);
    // seg->L에 덮어쓰기
    mzd_copy(seg->L, tmp);
    mzd_free(tmp);
}


//------------------------------------------------------------------------------
// free_LSegment: release L and row
//------------------------------------------------------------------------------
void free_LSegment(LSegment* seg) {
    mzd_free(seg->L);
    mzd_free(seg->row);
}

//------------------------------------------------------------------------------
// build_linear_system_with_pattern: build C (208×656)
//------------------------------------------------------------------------------
void build_linear_system_with_pattern(const uint8_t* pattern,
                                      mzd_t*        C)
{

    if (C == NULL) {
        fprintf(stderr, "build_linear_system_with_pattern: C must be preallocated\n");  
        abort();}   
        // 오로지 C가 0행렬인지 검사
    if (!mzd_is_zero(C)) {
        fprintf(stderr, "Error: C must be zero matrix\n");
        abort();
    }
  //verify_companion_matrices();

    LSegment s1, s2, s3;
    // 1) 세 LSegment 초기화
    init_LSegment(&s1, VAR_OFF_R1, VAR_LEN_R1, 1);
    init_LSegment(&s2, VAR_OFF_R2, VAR_LEN_R2, 2);
    init_LSegment(&s3, VAR_OFF_R3, VAR_LEN_R3, 3);

    // ─────────────────────────────────────────────────
    // 패턴에 따라 LSegment 갱신 및 dbg_y 기록
for (int i = 0; i < DISCARD + 208; ++i) {
    uint8_t p = pattern[i];

    // ─────────────────────────────────────────────────
    // 1) 패턴에 따라 L 업데이트
    // update_LSegment() 함수는 companion 행렬 A를 곱해
    // state.L (레지스터 상태 행렬 L)을 갱신합니다.
    char pb2 = (p & 0b100) ? '1' : '0';
    char pb1 = (p & 0b010) ? '1' : '0';
    char pb0 = (p & 0b001) ? '1' : '0';


    if (p & 0b100) update_LSegment(&s1);  // s1.L ← A1 × s1.L
    if (p & 0b010) update_LSegment(&s2);  // s2.L ← A2 × s2.L
    if (p & 0b001) update_LSegment(&s3);  // s3.L ← A3 × s3.L
    // ─────────────────────────────────────────────────
    // 2) discard 이후에만 row 계산 및 합산
    if (i >= DISCARD) {
        int j = i - DISCARD;

        // compute_segment_row() 함수는 주어진 L로부터
        // 수학적으로 결정된 segment row 벡터를 생성합니다.
        compute_segment_row(&s1);  // s1.row = f(s1.L)
        compute_segment_row(&s2);  // s2.row = f(s2.L)
        compute_segment_row(&s3);  // s3.row = f(s3.L)

        // C 행렬의 j번째 행 창(window) 가져오기
        mzd_t* row_win = mzd_init_window(
            C, j, 0,
            j + 1, TOTAL_VARS
        );

        // 창은 반드시 깨끗해야 한다
        if (!mzd_is_zero(row_win)) {
            fprintf(stderr, "Error: row_win[%d] is not zero!\n", j);
            abort();
        }

        // 세 segment row를 더해서 최종 row_win 완성
        mzd_add(row_win, row_win, s1.row);
        mzd_add(row_win, row_win, s2.row);
        mzd_add(row_win, row_win, s3.row);


        mzd_free_window(row_win);
    }
}


    free_LSegment(&s1);
    free_LSegment(&s2);
    free_LSegment(&s3);
}

//------------------------------------------------------------------------------
// generate_keystream_via_linear_system
//------------------------------------------------------------------------------


void generate_keystream_via_linear_system(lfsr_matrix_state_t* state,
                                          mzd_t*               z_vec)
{
    // 1) pattern
    init_clock_patterns();
    uint16_t r4_index = 0;
    for (int k = 1; k < 17; k++)
        r4_index |= mzd_read_bit(state->R4, 0, k) << (k - 1);
    const uint8_t* pattern = get_clock_pattern(r4_index);
    
    // 2) build C (208×TOTAL_VARS) and check
    mzd_t* C = mzd_init(208, TOTAL_VARS);

    // 3) extract v (1×TOTAL_VARS) and check
    extract_variables_from_state(state);
    assert(state->v && state->v->nrows == 1);
    assert(state->v->ncols == TOTAL_VARS);


    build_linear_system_with_pattern(pattern, C);
    assert(C->nrows == 208);
    assert(C->ncols == TOTAL_VARS);

    // 4) transpose C → Ct (TOTAL_VARS×208) and check
    mzd_t* Ct = mzd_init(TOTAL_VARS, 208);
    mzd_transpose(Ct, C);
    assert(Ct->nrows == TOTAL_VARS);
    assert(Ct->ncols == 208);

    // 5) ensure z_vec is 1×CIPHERTEXT_SIZE
    assert(z_vec && z_vec->nrows == 1);
    assert(z_vec->ncols == CIPHERTEXT_SIZE);

    // 6) multiply: (1×TOTAL_VARS) × (TOTAL_VARS×208) → (1×208)
    mzd_mul(z_vec, state->v, Ct, 0);

    // 7) cleanup
    mzd_free(C);
    mzd_free(Ct);
}









int find_first_one_from_1(const mzd_t* row) {
    int ncols = row->ncols;
    for (int j = 1; j < ncols; ++j) {
        if (mzd_read_bit(row, 0, j)) {
            return j;
        }
    }
    return -1;
}

/**
 * Print the first '1' bit position in the row (searching from j=1),
 * or a message if none.
 */
void print_first_one_from_1(const mzd_t* row) {
    int idx = find_first_one_from_1(row);
    if (idx >= 0) {
        printf("First 1 from column 1 at index %d\n", idx);
    } else {
        printf("No 1 bit found from column 1 onward\n");
    }
}
 
void load_cipher_noscramble_m4ri(
    const char* cipher_path,
    const char* scramble_path,
    mzd_t*      c_vecs[NUM_BLOCKS]
) {
    // 파일 열기 및 크기 검증
    FILE* fc = fopen(cipher_path, "rb");
    if (!fc) { perror(cipher_path); exit(EXIT_FAILURE); }
    fseek(fc, 0, SEEK_END);
    long cipher_size = ftell(fc);
    if (cipher_size != NUM_BLOCKS * BLOCK_BYTES) {
        fprintf(stderr,
                "Error: %s size %ld != expected %d\n",
                cipher_path, cipher_size, NUM_BLOCKS * BLOCK_BYTES);
        exit(EXIT_FAILURE);
    }
    fseek(fc, 0, SEEK_SET);

    FILE* fs = fopen(scramble_path, "rb");
    if (!fs) { perror(scramble_path); exit(EXIT_FAILURE); }
    fseek(fs, 0, SEEK_END);
    long scramble_size = ftell(fs);
    if (scramble_size != BLOCK_BYTES) {
        fprintf(stderr,
                "Error: %s size %ld != expected %d\n",
                scramble_path, scramble_size, BLOCK_BYTES);
        exit(EXIT_FAILURE);
    }
    fseek(fs, 0, SEEK_SET);

    // scramble 벡터 읽기
    unsigned char s_bytes[BLOCK_BYTES];
    if (fread(s_bytes, 1, BLOCK_BYTES, fs) != BLOCK_BYTES) {
        fprintf(stderr, "Error: failed to read %s\n", scramble_path);
        exit(EXIT_FAILURE);
    }
    fclose(fs);

    // 비트 단위로 분해
    int s_bits[CIPHERTEXT_SIZE];
    for (int b = 0; b < BLOCK_BYTES; b++) {
        for (int k = 0; k < 8; k++) {
            s_bits[b*8 + k] = (s_bytes[b] >> (7 - k)) & 1;
        }
    }

    // 각 블럭에 대해, cipher 읽고 scramble 제거 후 m4ri 벡터에 저장
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // 1×CIPHERTEXT_SIZE 벡터 생성
        mzd_t* vec = mzd_init(1, CIPHERTEXT_SIZE);
        if (!vec) {
            fprintf(stderr, "Error: mzd_init failed for block %d\n", i);
            exit(EXIT_FAILURE);
        }

        for (int b = 0; b < BLOCK_BYTES; b++) {
            unsigned char w;
            if (fread(&w, 1, 1, fc) != 1) {
                fprintf(stderr, "Error: failed to read cipher byte %d of block %d\n", b, i);
                exit(EXIT_FAILURE);
            }
            for (int k = 0; k < 8; k++) {
                int bit = ((w >> (7 - k)) & 1) ^ s_bits[b*8 + k];
                mzd_write_bit(vec, 0, b*8 + k, bit);
            }
        }

        c_vecs[i] = vec;
    }

    fclose(fc);

}
/**
 * Initialize the c_vecs array with mzd_t structures.
 * Each vector is initialized to 1 row and CIPHERTEXT_SIZE columns.
 */
// 초기화 함수 구현
void init_c_vecs(void) {
    // load_cipher_noscramble_m4ri는 
    // CIPHERTEXT_PATH에서 읽고 SCRAMBLE_PATH를 제거하여 c_vecs를 채웁니다.
    load_cipher_noscramble_m4ri(
        CIPHERTEXT_PATH,
        SCRAMBLE_PATH,
        c_vecs
    );
}
unsigned char *load_packed_bin(const char *path, size_t *out_bytes) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    unsigned char *buf = malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf,1,sz,f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_bytes = (size_t)sz;
    return buf;
}

mzd_t *load_packed_matrix(const char *path, int rows, int cols) {
    size_t bytes;
    unsigned char *buf = load_packed_bin(path, &bytes);
    if (!buf) return NULL;

    size_t expected = ((size_t)rows * cols + 7) / 8;
    if (bytes != expected) {
        free(buf);
        return NULL;
    }

    mzd_t *M = mzd_init(rows, cols);
    if (!M) {
        free(buf);
        return NULL;
    }

    // row-major, MSB-first unpack
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            size_t idx    = (size_t)r * cols + c;
            size_t byte_i = idx >> 3;
            int    bit_i  = 7 - (idx & 7);
            int    bit    = (buf[byte_i] >> bit_i) & 1;
            mzd_write_bit(M, r, c, bit);
        }
    }

    free(buf);
    return M;
}


int test_ct_build(void) {
    clock_t t0, t1;
    init_clock_patterns();
    lfsr_matrices_init();

    printf("Building %u Ct matrices: 0%%", R4_SPACE);
    fflush(stdout);

    t0 = clock();
    for (uint32_t r4 = 0; r4 < R4_SPACE; r4++) {
        mzd_t *Ct = mzd_init(TOTAL_VARS, C_ROWS);
        get_Ct_for_r4(r4,Ct);
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
    return 0;
}

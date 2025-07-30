#include "decrypt.h"
//------------------------------------------------------------------------------

// Computes dbg_y[i+1] by XOR’ing the 4 tap‐bits from each LSegment’s companion matrix L
// Equivalent to:
//   lfsr_matrix_get(R1,1) ^ lfsr_matrix_get(R1,6) ^ lfsr_matrix_get(R1,15) ^ lfsr_matrix_get(R1,11)
// ^ lfsr_matrix_get(R2,3) ^ lfsr_matrix_get(R2,8) ^ lfsr_matrix_get(R2,14)  ^ lfsr_matrix_get(R2,1)
// ^ lfsr_matrix_get(R3,4) ^ lfsr_matrix_get(R3,15) ^ lfsr_matrix_get(R3,19)  ^ lfsr_matrix_get(R3,0)


// In your .c file, replace the previous helper with this corrected C version:

// 사용 예시:
// init_lfsr_matrices();
// verify_companion_matrices();


// 실제 LUT를 채우는 내부 함수
static uint8_t cross3_LUT[8][8];
static bool    cross3_ready = false;

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
        mzd_write_bit(seg->row, 0, 0, c0);
    }

    // 4) Linear terms → seg->var_offset + u, for u=1..r-1
    for (int u = 1; u < r; ++u) {
        int c_lin =
            cross3_lut(taps[u], taps[0])       // M[u][0]
          ^ cross3_lut(taps[0], taps[u])       // M[0][u]
          ^ cross3_lut(taps[u], taps[u])       // M[u][u]
          ^ l4[u];                             // ⊕ L4[u]

        mzd_write_bit(seg->row, 0, seg->var_offset + u-1, c_lin);
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
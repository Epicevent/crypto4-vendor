#include "encrypt.h" //

mzd_t *mzd_identity_bit(int n) {
    mzd_t *I = mzd_init(n, n);
    if (!I) return NULL;
    for (int i = 0; i < n; i++) {
        mzd_write_bit(I, i, i, 1);
    }
    return I;
}



// 키스케줄: key_vec(1x64), nonce_vec(1x19) -> a_vec(1x64)
void key_scheduling_m4ri(const mzd_t* key_vec, const mzd_t* nonce_vec, mzd_t* a_vec) {
    // a[i] = K[i]
    for (int i = 0; i < 64; i++) {
        int bit = mzd_read_bit(key_vec, 0, i);
        mzd_write_bit(a_vec, 0, i, bit);
    }
    // for (int i = 3; i <= 15; i++) a[i] = a[i] ^ N[i + 3];
    for (int i = 3; i <= 15; i++) {
        int a_bit = mzd_read_bit(a_vec, 0, i);
        int n_bit = mzd_read_bit(nonce_vec, 0, i + 3);
        mzd_write_bit(a_vec, 0, i, a_bit ^ n_bit);
    }
    // for (int i = 22; i <= 23; i++) a[i] = a[i] ^ N[i - 18];
    for (int i = 22; i <= 23; i++) {
        int a_bit = mzd_read_bit(a_vec, 0, i);
        int n_bit = mzd_read_bit(nonce_vec, 0, i - 18);
        mzd_write_bit(a_vec, 0, i, a_bit ^ n_bit);
    }
    // for (int i = 60; i <= 63; i++) a[i] = a[i] ^ N[i - 60];
    for (int i = 60; i <= 63; i++) {
        int a_bit = mzd_read_bit(a_vec, 0, i);
        int n_bit = mzd_read_bit(nonce_vec, 0, i - 60);
        mzd_write_bit(a_vec, 0, i, a_bit ^ n_bit);
    }
}

// 비트 리버설: a_vec(1x64) -> aa_vec(1x64)
void bit_reversal_m4ri(const mzd_t* a_vec, mzd_t* aa_vec) {
    // 4블록(16비트씩) 단위로 비트 순서 뒤집기
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 16; j++) {
            int bit = mzd_read_bit(a_vec, 0, i * 16 + j);
            mzd_write_bit(aa_vec, 0, i * 16 + 15 - j, bit);
        }
    }
}


// 키 인젝션: aa_vec(1x64), state -> state
void key_injection_m4ri(const mzd_t* aa_vec, lfsr_matrix_state_t* state) {
    // 전역 행렬이 초기화되지 않았으면 초기화
    if (!A1 || !A2 || !A3 || !A4) {
        lfsr_matrices_init();
    }
    
    for (int k = 0; k < 64; k++) {
        lfsr_matrix_clock(state->R1, A1);
        lfsr_matrix_clock(state->R2, A2);
        lfsr_matrix_clock(state->R3, A3);
        lfsr_matrix_clock(state->R4, A4);
        int aa_bit = mzd_read_bit(aa_vec, 0, k);
        if (aa_bit) {
            // 각 LFSR의 0번째 비트에 1 XOR
            mzd_write_bit(state->R1, 0, 0, mzd_read_bit(state->R1, 0, 0) ^ 1);
            mzd_write_bit(state->R2, 0, 0, mzd_read_bit(state->R2, 0, 0) ^ 1);
            mzd_write_bit(state->R3, 0, 0, mzd_read_bit(state->R3, 0, 0) ^ 1);
            mzd_write_bit(state->R4, 0, 0, mzd_read_bit(state->R4, 0, 0) ^ 1);
        }
    }
    // 마지막에 각 LFSR의 0번째 비트를 1로 강제
    mzd_write_bit(state->R1, 0, 0, 1);
    mzd_write_bit(state->R2, 0, 0, 1);
    mzd_write_bit(state->R3, 0, 0, 1);
    mzd_write_bit(state->R4, 0, 0, 1);
}

// 상태 확장: S0_state, num, S_states[num] (선형화된 버전으로 대체)
void expand_states_from_initial_m4ri(const lfsr_matrix_state_t* S0, int num, lfsr_matrix_state_t** S_states) {
    // 선형화된 확장 함수를 호출
    expand_states_linearized_m4ri(S0, num, S_states);
}

// LSB 정규화 함수
void normalize_lsb(lfsr_matrix_state_t* state) {
    if (!state) return;
    mzd_write_bit(state->R1, 0, 0, 1);
    mzd_write_bit(state->R2, 0, 0, 1);
    mzd_write_bit(state->R3, 0, 0, 1);
    mzd_write_bit(state->R4, 0, 0, 1);
}

// keystream 생성: state, pattern -> z_vec(1x208)
void keystream_generation_with_pattern_m4ri(const lfsr_matrix_state_t* state, const uint8_t* pattern, mzd_t* z_vec) {
    // 전역 행렬이 초기화되지 않았으면 초기화
    if (!A1 || !A2 || !A3) {
        lfsr_matrices_init();
    }
    
    const int discard = 250;
    mzd_t* R1 = mzd_copy(NULL, state->R1);
    mzd_t* R2 = mzd_copy(NULL, state->R2);
    mzd_t* R3 = mzd_copy(NULL, state->R3);



    for (int i = 0; i < discard + 208; i++) {
        uint8_t p = pattern[i];
        if (p & 0b100) lfsr_matrix_clock(R1, A1);
        if (p & 0b010) lfsr_matrix_clock(R2, A2);
        if (p & 0b001) lfsr_matrix_clock(R3, A3);
        // ─────────────────────────────────────────────────


        if (i >= discard) {
            int maj1 = majority_matrix(lfsr_matrix_get(R1, 1), lfsr_matrix_get(R1, 6), lfsr_matrix_get(R1, 15));
            int maj2 = majority_matrix(lfsr_matrix_get(R2, 3), lfsr_matrix_get(R2, 8), lfsr_matrix_get(R2, 14));
            int maj3 = majority_matrix(lfsr_matrix_get(R3, 4), lfsr_matrix_get(R3, 15), lfsr_matrix_get(R3, 19));
            int z_bit = maj1 ^ maj2 ^ maj3 ^ lfsr_matrix_get(R1, 11) ^ lfsr_matrix_get(R2, 1) ^ lfsr_matrix_get(R3, 0);
            mzd_write_bit(z_vec, 0, i - discard, z_bit);

        }
    }
    

    mzd_free(R1); mzd_free(R2); mzd_free(R3);
}








// --- 신뢰 가능한 인코딩 파트: 평문 -> e_all ---
void encoding_part_m4ri(const char* plaintext, int* e_all, int num_blocks, const int* Gt) {
    encode_plaintext_m4ri(plaintext, e_all, num_blocks, Gt);
}
void encode_plaintext_m4ri(const char* plaintext, int* e, int num_blocks, const int* Gt) {
    // Gt: (CIPHERTEXT_SIZE x PLAINTEXT_BLOCK_SIZE) row-major
    static mzd_t* Gt_mat = NULL;
    if (!Gt_mat) {
        Gt_mat = mzd_init(PLAINTEXT_BLOCK_SIZE, CIPHERTEXT_SIZE);
        for (int i = 0; i < PLAINTEXT_BLOCK_SIZE; i++) {
            for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
                mzd_write_bit(Gt_mat, i, j, Gt[j * PLAINTEXT_BLOCK_SIZE + i]);
            }
        }
    }
    for (int blk = 0; blk < num_blocks; blk++) {
        // 1. 평문 블록을 m4ri 벡터로 변환
        mzd_t* p_vec = mzd_init(1, PLAINTEXT_BLOCK_SIZE);
        for (int i = 0; i < 20; i++) {
            unsigned char w = (unsigned char)plaintext[blk * 20 + i];
            for (int k = 0; k < 8; k++) {
                mzd_write_bit(p_vec, 0, i * 8 + k, (w >> (7 - k)) & 1);
            }
        }
        // 2. e = p * Gt (m4ri 곱셈)
        mzd_t* e_vec = mzd_init(1, CIPHERTEXT_SIZE);
        mzd_mul(e_vec, p_vec, Gt_mat, 0);
        // 3. 결과를 int 배열로 복사
        for (int i = 0; i < CIPHERTEXT_SIZE; i++) {
            e[blk * CIPHERTEXT_SIZE + i] = mzd_read_bit(e_vec, 0, i);
        }
        mzd_free(p_vec);
        mzd_free(e_vec);
    }
}

void encrypt_from_state_precise_m4ri(
    int R1_init, int R2_init, int R3_init, int R4_init,
    const char* plaintext,
    int err1, int err2, int err1_bit, int err2_bit,
    int* c_out,
    const int* s, const int* Gt,
    int* z_out
) {
    // 1) 초기 LFSR 상태 설정 (변경 없음)
    int S0[4] = {R1_init, R2_init, R3_init, R4_init};
    lfsr_matrix_state_t S0_state = {0};
    S0_state.R1 = mzd_init(1, 19);
    S0_state.R2 = mzd_init(1, 22);
    S0_state.R3 = mzd_init(1, 23);
    S0_state.R4 = mzd_init(1, 17);
    for (int i = 0; i < 19; i++)
        mzd_write_bit(S0_state.R1, 0, i, (S0[0] >> i) & 1);
    for (int i = 0; i < 22; i++)
        mzd_write_bit(S0_state.R2, 0, i, (S0[1] >> i) & 1);
    for (int i = 0; i < 23; i++)
        mzd_write_bit(S0_state.R3, 0, i, (S0[2] >> i) & 1);
    for (int i = 0; i < 17; i++)
        mzd_write_bit(S0_state.R4, 0, i, (S0[3] >> i) & 1);

    // 2) 모든 블록에 대한 상태 배열과 인코딩 생성
    lfsr_matrix_state_t* S_states[NUM_BLOCKS];
    expand_states_from_initial_m4ri(&S0_state, NUM_BLOCKS, S_states);

    int e_all[NUM_BLOCKS * CIPHERTEXT_SIZE];
    encoding_part_m4ri(plaintext, e_all, NUM_BLOCKS, Gt);

    // 3) 패턴 테이블 로드 (스레드-세이프 init이 구현되어 있으면 호출 생략 가능)
    init_clock_patterns();

    // 4) 각 블록별 암호화 루프
    for (int i = 0; i < NUM_BLOCKS; i++) {
        lfsr_matrix_state_t* state = S_states[i];
        if (!state || !state->R1 || !state->R2 || !state->R3 || !state->R4) {
            fprintf(stderr, "[m4ri] S_states[%d] NULL!\n", i);
            abort();
        }

        // R4 비트 1~16 조합 → 인덱스
        uint16_t r4_index = 0;
        for (int k = 1; k < 17; k++)
            r4_index |= mzd_read_bit(state->R4, 0, k) << (k - 1);

        // 패턴 조회
        const uint8_t* pattern = get_clock_pattern(r4_index);

        // 키스트림 생성
        mzd_t* keystream_vec = mzd_init(1, CIPHERTEXT_SIZE);
        keystream_generation_with_pattern_m4ri(state, pattern, keystream_vec);

        // 암호문 벡터 생성
        mzd_t* c_vec = mzd_init(1, CIPHERTEXT_SIZE);
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            int ebit = e_all[i * CIPHERTEXT_SIZE + j];
            int zbit = mzd_read_bit(keystream_vec, 0, j);
            int sbit = s[j];
            mzd_write_bit(c_vec, 0, j, ebit ^ zbit ^ sbit);
        }

        // 에러 인젝션
        if (err1 == i && err1_bit < CIPHERTEXT_SIZE) {
            int b = mzd_read_bit(c_vec, 0, err1_bit);
            mzd_write_bit(c_vec, 0, err1_bit, b ^ 1);
        }
        if (err2 == i && err2_bit < CIPHERTEXT_SIZE) {
            int b = mzd_read_bit(c_vec, 0, err2_bit);
            mzd_write_bit(c_vec, 0, err2_bit, b ^ 1);
        }

        // 출력 복사
        for (int j = 0; j < CIPHERTEXT_SIZE; j++)
            c_out[i * CIPHERTEXT_SIZE + j] = mzd_read_bit(c_vec, 0, j);

        for (int j = 0; j < CIPHERTEXT_SIZE; j++)
            z_out[i * CIPHERTEXT_SIZE + j] = mzd_read_bit(keystream_vec, 0, j);

        mzd_free(keystream_vec);
        mzd_free(c_vec);
    }

    // 5) 초기 상태 해제
    mzd_free(S0_state.R1);
    mzd_free(S0_state.R2);
    mzd_free(S0_state.R3);
    mzd_free(S0_state.R4);
}


// --- 선형화된 상태 확장 (zS.bin 사용) ---
void expand_states_linearized_m4ri(const lfsr_matrix_state_t* S0, int num, lfsr_matrix_state_t** S_states) {
    // 전역 zS가 초기화되지 않았으면 초기화
    if (!zS_R1) {
        lfsr_matrices_init();
    }
    
    // 각 S_states 생성 (진짜 행렬 연산)
    for (int i = 0; i < num; i++) {
        if (!S_states[i]) {
            // calloc으로 할당하면 모든 필드(R1,R2,R3,R4,v)가 0(NULL)으로 초기화됩니다.
            S_states[i] = calloc(1, sizeof(lfsr_matrix_state_t));
            if (!S_states[i]) {
                fprintf(stderr, "[m4ri] S_states[%d] calloc failed\n", i);
                abort();
            }
        }
        lfsr_matrix_initialization(S_states[i]);
        if (i == 0) {
            // S[0] = S0
            mzd_copy(S_states[i]->R1, S0->R1);
            mzd_copy(S_states[i]->R2, S0->R2);
            mzd_copy(S_states[i]->R3, S0->R3);
            mzd_copy(S_states[i]->R4, S0->R4);
        } else {
            // S[i] = S0 ⊕ zS[i-1] (진짜 행렬 연산)
            // zS의 (i-1)번째 row를 추출하여 S0와 XOR
            
            // R1: zS_R1의 (i-1)번째 row를 추출
            mzd_t* zS_row_R1 = mzd_init_window(zS_R1, i-1, 0, i, 19);
            mzd_add(S_states[i]->R1, S0->R1, zS_row_R1);
            mzd_free(zS_row_R1);
            
            // R2: zS_R2의 (i-1)번째 row를 추출  
            mzd_t* zS_row_R2 = mzd_init_window(zS_R2, i-1, 0, i, 22);
            mzd_add(S_states[i]->R2, S0->R2, zS_row_R2);
            mzd_free(zS_row_R2);
            
            // R3: zS_R3의 (i-1)번째 row를 추출
            mzd_t* zS_row_R3 = mzd_init_window(zS_R3, i-1, 0, i, 23);
            mzd_add(S_states[i]->R3, S0->R3, zS_row_R3);
            mzd_free(zS_row_R3);
            
            // R4: zS_R4의 (i-1)번째 row를 추출
            mzd_t* zS_row_R4 = mzd_init_window(zS_R4, i-1, 0, i, 17);
            mzd_add(S_states[i]->R4, S0->R4, zS_row_R4);
            mzd_free(zS_row_R4);
        }
        
        // LSB 정규화 (별도 함수)
        normalize_lsb(S_states[i]);
    }
}

void lfsr_enc_m4ri(
    const int K[KEY_SIZE],
    const int N[NONCE_SIZE],
    int z[CIPHERTEXT_SIZE]
) {
    // 1) 키·논스 벡터 초기화
    mzd_t* key_vec   = mzd_init(1, KEY_SIZE);
    mzd_t* nonce_vec = mzd_init(1, NONCE_SIZE);
    for (int i = 0; i < KEY_SIZE; i++)
        mzd_write_bit(key_vec,   0, i, K[i]);
    for (int i = 0; i < NONCE_SIZE; i++)
        mzd_write_bit(nonce_vec, 0, i, N[i]);

    // 2) 스케줄링 & 비트 반전
    mzd_t* a_vec  = mzd_init(1, KEY_SIZE);
    mzd_t* aa_vec = mzd_init(1, KEY_SIZE);
    key_scheduling_m4ri(key_vec, nonce_vec, a_vec);
    bit_reversal_m4ri(a_vec, aa_vec);

    // 3) LFSR 상태 초기화 및 키 주입
    lfsr_matrix_state_t state;
    lfsr_matrix_initialization(&state);
    key_injection_m4ri(aa_vec, &state);

    // 4) R4 인덱스 계산
    uint16_t r4_index = 0;
    for (int k = 1; k < 17; k++)
        r4_index |= mzd_read_bit(state.R4, 0, k) << (k - 1);

    // 5) 패턴 테이블 초기화·조회
    init_clock_patterns();
    const uint8_t* pattern = get_clock_pattern(r4_index);

    // 6) 키스트림 생성
    mzd_t* z_vec = mzd_init(1, CIPHERTEXT_SIZE);
    keystream_generation_with_pattern_m4ri(&state, pattern, z_vec);

    // 7) 출력 배열에 비트 복사
    for (int j = 0; j < CIPHERTEXT_SIZE; j++)
        z[j] = mzd_read_bit(z_vec, 0, j);

    // 8) 리소스 해제
    mzd_free(z_vec);
    mzd_free(key_vec);
    mzd_free(nonce_vec);
    mzd_free(a_vec);
    mzd_free(aa_vec);
    mzd_free(state.R1);
    mzd_free(state.R2);
    mzd_free(state.R3);
    mzd_free(state.R4);
}

// m4ri 기반: my_encrypt와 동일한 시그니처의 암호화 함수
void encrypt_m4ri(const int key[KEY_SIZE], const char* plaintext, int err1, int err2, int err1_bit, int err2_bit, int* ciphertext, const int* s, const int* Gt) {
    int FN = 9867;
    int N[NUM_BLOCKS][NONCE_SIZE];
    int p[NUM_BLOCKS][PLAINTEXT_BLOCK_SIZE];
    int e[NUM_BLOCKS][CIPHERTEXT_SIZE];
    int z[NUM_BLOCKS][CIPHERTEXT_SIZE];
    int c[NUM_BLOCKS][CIPHERTEXT_SIZE];

    // 1. nonce 생성 (my_encrypt와 동일)
    for (int i = 0; i < NUM_BLOCKS; i++) {
        for (int j = 0; j < NONCE_SIZE; j++) {
            N[i][j] = ((FN + i) >> j) & 1;
        }
    }

    // 2. 평문을 비트로 변환 (my_encrypt와 동일)
    for (int i = 0; i < NUM_BLOCKS; i++) {
        for (int j = 0; j < 20; j++) {
            unsigned char w = (unsigned char)plaintext[i * 20 + j];
            for (int k = 0; k < 8; k++) {
                p[i][j * 8 + k] = (w >> (7 - k)) & 1;
            }
        }
    }

    // 3. e 계산 (my_encrypt와 동일)
    for (int i = 0; i < NUM_BLOCKS; i++) {
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            e[i][j] = 0;
            for (int k = 0; k < PLAINTEXT_BLOCK_SIZE; k++) {
                e[i][j] ^= Gt[j*PLAINTEXT_BLOCK_SIZE + k] & p[i][k];
            }
        }
    }

    // 4. 각 블록별로 키스트림 생성 (기존 lfsr_enc 함수 재사용)
    for (int i = 0; i < NUM_BLOCKS; i++) {
        lfsr_enc_m4ri(key, N[i], z[i]);
    }

    // 5. 암호문 결합 (my_encrypt와 동일)
    for (int i = 0; i < NUM_BLOCKS; i++) {
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            c[i][j] = e[i][j] ^ z[i][j] ^ s[j];
        }
    }

    // 6. 에러 비트 적용 (my_encrypt와 동일)
    if (err1 >= 0 && err1 < NUM_BLOCKS && err1_bit >= 0 && err1_bit < CIPHERTEXT_SIZE)
        c[err1][err1_bit] ^= 1;
    if (err2 >= 0 && err2 < NUM_BLOCKS && err2_bit >= 0 && err2_bit < CIPHERTEXT_SIZE)
        c[err2][err2_bit] ^= 1;

    // 7. 결과를 1차원 배열로 복사 (my_encrypt와 동일)
    for (int i = 0; i < NUM_BLOCKS; i++) {
        for (int j = 0; j < CIPHERTEXT_SIZE; j++) {
            ciphertext[i * CIPHERTEXT_SIZE + j] = c[i][j];
        }
    }
} 

void generate_keystream_via_clocks( lfsr_matrix_state_t* state,
                                   mzd_t*                     z_vec)
{
    // 1) 패턴 테이블 초기화
    init_clock_patterns();

    // 2) state->R4(17비트)로부터 r4_index 계산
    // 4) R4 인덱스 계산
    uint16_t r4_index = 0;
    for (int k = 1; k < 17; k++)
        r4_index |= mzd_read_bit(state->R4, 0, k) << (k - 1);

    // 3) r4_index로부터 clock 패턴 조회
    const uint8_t* pattern = get_clock_pattern(r4_index);

    // 4) 기존 keystream 생성 함수 재사용
    //    z_vec은 이미 mzd_init(1, CIPHERTEXT_SIZE) 상태여야 합니다.
    keystream_generation_with_pattern_m4ri(state, pattern, z_vec);
}












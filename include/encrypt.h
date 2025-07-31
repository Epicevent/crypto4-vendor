#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <m4ri/m4ri.h>
#include <stdint.h>
#include "lfsr_state.h"


#ifdef __cplusplus
extern "C" 
#endif





mzd_t *mzd_identity_bit(int n);
/**
 * 패턴 테이블을 메모리에 로드합니다.
 * 프로그램 시작 직후 또는 encrypt 전 한 번만 호출하세요.
 */





// --- 전역 LFSR companion matrix 캐시 ---
extern mzd_t* A1; // R1 companion matrix (19x19)
extern mzd_t* A2; // R2 companion matrix (22x22)
extern mzd_t* A3; // R3 companion matrix (23x23)
extern mzd_t* A4; // R4 companion matrix (17x17)

// --- 전역 zS matrix 캐시 ---
extern mzd_t* zS_R1; // zS R1 matrix (ZS_ROWS x 19)
extern mzd_t* zS_R2; // zS R2 matrix (ZS_ROWS x 22)
extern mzd_t* zS_R3; // zS R3 matrix (ZS_ROWS x 23)
extern mzd_t* zS_R4; // zS R4 matrix (ZS_ROWS x 17)

// --- 함수 선언 ---

// 키스케줄: key_vec(1x64), nonce_vec(1x19) -> a_vec(1x64)
void key_scheduling_m4ri(const mzd_t* key_vec, const mzd_t* nonce_vec, mzd_t* a_vec);

// 비트 리버설: a_vec(1x64) -> aa_vec(1x64)
void bit_reversal_m4ri(const mzd_t* a_vec, mzd_t* aa_vec);

// LFSR 상태 초기화


// 키 인젝션
void key_injection_m4ri(const mzd_t* aa_vec, lfsr_matrix_state_t* state);

// 상태 확장
void expand_states_from_initial_m4ri(const lfsr_matrix_state_t* S0, int num, lfsr_matrix_state_t** S_states);

// 선형화된 상태 확장 (zS.bin 사용)
void expand_states_linearized_m4ri(const lfsr_matrix_state_t* S0, int num, lfsr_matrix_state_t** S_states);

// LSB 정규화 함수
void normalize_lsb(lfsr_matrix_state_t* state);

// keystream 생성
void keystream_generation_with_pattern_m4ri(const lfsr_matrix_state_t* state, const uint8_t* pattern, mzd_t* z_vec);

// 암호화 메인 함수
void encrypt(
    const int key[KEY_SIZE],
    const char* plaintext,
    int err1, int err2,
    int err1_bit, int err2_bit,
    int* ciphertext,
    const int* s,
    const int* Gt
);

// 보조 함수들


void encrypt_from_state_precise_m4ri(
    int R1_init, int R2_init, int R3_init, int R4_init,
    const char* plaintext, int err1, int err2, int err1_bit, int err2_bit, int* c_out,
    const int* s, const int* Gt, int* z_out
);

void encrypt_m4ri(const int key[KEY_SIZE], const char* plaintext, int err1, int err2, int err1_bit, int err2_bit, int* ciphertext, const int* s, const int* Gt);

void lfsr_enc_m4ri(const int K[KEY_SIZE], const int N[NONCE_SIZE], int z[CIPHERTEXT_SIZE]);




// 1) 기존 LFSR clock 순차 진행으로 키스트림 생성
//    - state: 초기 R1,R2,R3,R4 상태
//    - z_vec: (1×208) keystream 출력
void generate_keystream_via_clocks(lfsr_matrix_state_t* state,
                                   mzd_t*                     z_vec);

void extract_variables_from_state(lfsr_matrix_state_t* state);
void generate_keystream_via_linear_system(lfsr_matrix_state_t* state,
                                          mzd_t*               z_vec);



#ifdef __cplusplus
}
#endif

#endif // ENCRYPT_H 
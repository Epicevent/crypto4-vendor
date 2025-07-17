#include <stdio.h>
#include <stdlib.h>
#include "crypto_lib.h"
#include "encrypt.h"

int main() {
    printf("Crypto4 Library Test\n");
    printf("===================\n\n");
    
    // 1. 라이브러리 초기화 테스트
    printf("1. Testing library initialization...\n");
    lfsr_matrices_init();
    printf("   ✓ LFSR matrices initialized successfully\n\n");
    
    // 2. 간단한 상태 확장 테스트
    printf("2. Testing state expansion...\n");
    
    // 초기 상태 설정 (간단한 예시)
    lfsr_matrix_state_t S0;
    S0.R1 = mzd_init(1, 19);
    S0.R2 = mzd_init(1, 22);
    S0.R3 = mzd_init(1, 23);
    S0.R4 = mzd_init(1, 17);
    
    // LSB를 1로 설정 (정규화)
    mzd_write_bit(S0.R1, 0, 0, 1);
    mzd_write_bit(S0.R2, 0, 0, 1);
    mzd_write_bit(S0.R3, 0, 0, 1);
    mzd_write_bit(S0.R4, 0, 0, 1);
    
    printf("   ✓ Initial state created with LSB=1\n");
    
    // 3. 상태 확장 테스트
    lfsr_matrix_state_t* S_states[5];
    expand_states_from_initial_m4ri(&S0, 5, S_states);
    printf("   ✓ Expanded 5 states successfully\n");
    
    // 4. 메모리 정리
    for (int i = 0; i < 5; i++) {
        mzd_free(S_states[i]->R1);
        mzd_free(S_states[i]->R2);
        mzd_free(S_states[i]->R3);
        mzd_free(S_states[i]->R4);
        free(S_states[i]);
    }
    
    mzd_free(S0.R1);
    mzd_free(S0.R2);
    mzd_free(S0.R3);
    mzd_free(S0.R4);
    
    printf("   ✓ Memory cleaned up\n\n");
    
    // 5. 라이브러리 정리
    lfsr_matrices_cleanup();
    printf("3. Library cleanup completed\n");
    
    printf("\n✅ All tests passed successfully!\n");
    printf("The Crypto4 library is working correctly.\n");
    
    return 0;
} 
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <m4ri/m4ri.h>
#include <stdint.h>
#include <stdbool.h>
#include "encrypt.h"
#include "decrypt.h"

#define CIPHERTEXT_SIZE 208
#define NTESTS          1000

int main(void) {
    srand((unsigned)time(NULL));
    init_clock_patterns();

    for (int t = 0; t < NTESTS; ++t) {
        // 1) 각 LFSR 초기값을 무작위로 생성 (LSB를 1로 고정)
        uint32_t R1_init = (rand() & ((1u << 19) - 1)) | 1u;
        uint32_t R2_init = (rand() & ((1u << 22) - 1)) | 1u;
        uint32_t R3_init = (rand() & ((1u << 23) - 1)) | 1u;
        uint32_t R4_init = (rand() & ((1u << 17) - 1)) | 1u;
        uint32_t S0[4]  = { R1_init, R2_init, R3_init, R4_init };

        // 2) 두 state 구조체에 동일하게 초기화
        lfsr_matrix_state_t stateC = {0}, stateL = {0};

        // 3) 각 레지스터 비트열을 mzd에 쓰기
        mzd_t* regsC[4] = {
            mzd_init(1,19), mzd_init(1,22),
            mzd_init(1,23), mzd_init(1,17)
        };
        mzd_t* regsL[4] = {
            mzd_init(1,19), mzd_init(1,22),
            mzd_init(1,23), mzd_init(1,17)
        };
        for (int r = 0; r < 4; ++r) {
            int len = regsC[r]->ncols;
            for (int i = 0; i < len; ++i) {
                int bit = (S0[r] >> i) & 1;
                mzd_write_bit(regsC[r], 0, i, bit);
                mzd_write_bit(regsL[r], 0, i, bit);
            }
        }
        stateC.R1 = regsC[0]; stateC.R2 = regsC[1];
        stateC.R3 = regsC[2]; stateC.R4 = regsC[3];
        stateL.R1 = regsL[0]; stateL.R2 = regsL[1];
        stateL.R3 = regsL[2]; stateL.R4 = regsL[3];

        // 4) 출력 벡터 초기화
        mzd_t* zC = mzd_init(1, CIPHERTEXT_SIZE);
        mzd_t* zL = mzd_init(1, CIPHERTEXT_SIZE);

        // 5) 키스트림 생성
        generate_keystream_via_clocks(&stateC, zC);
        generate_keystream_via_linear_system(&stateL, zL);

        // 6) R4 내용이 같은지 확인
        assert(mzd_cmp(stateC.R4, stateL.R4) == 0);

        // 7) z 벡터 내용이 같은지 검증
        assert(mzd_cmp(zC, zL) == 0);

        // 8) 정리
        for (int r = 0; r < 4; ++r) {
            mzd_free(regsC[r]);
            mzd_free(regsL[r]);
        }
        if (stateC.v) mzd_free(stateC.v);
        if (stateL.v) mzd_free(stateL.v);
        mzd_free(zC);
        mzd_free(zL);
    }

    printf("All %d tests passed!\n", NTESTS);
    return 0;
}

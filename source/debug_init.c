#include "encrypt.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Debug Init Test ===\n");
    
    printf("Step 1: Testing mzd_init for R1...\n");
    mzd_t* R1 = mzd_init(1, 19);
    if (!R1) {
        printf("FAILED: mzd_init(1, 19) returned NULL\n");
        return 1;
    }
    printf("SUCCESS: R1 initialized\n");
    
    printf("Step 2: Testing mzd_init for R2...\n");
    mzd_t* R2 = mzd_init(1, 22);
    if (!R2) {
        printf("FAILED: mzd_init(1, 22) returned NULL\n");
        mzd_free(R1);
        return 1;
    }
    printf("SUCCESS: R2 initialized\n");
    
    printf("Step 3: Testing mzd_init for R3...\n");
    mzd_t* R3 = mzd_init(1, 23);
    if (!R3) {
        printf("FAILED: mzd_init(1, 23) returned NULL\n");
        mzd_free(R1);
        mzd_free(R2);
        return 1;
    }
    printf("SUCCESS: R3 initialized\n");
    
    printf("Step 4: Testing mzd_init for R4...\n");
    mzd_t* R4 = mzd_init(1, 17);
    if (!R4) {
        printf("FAILED: mzd_init(1, 17) returned NULL\n");
        mzd_free(R1);
        mzd_free(R2);
        mzd_free(R3);
        return 1;
    }
    printf("SUCCESS: R4 initialized\n");
    
    printf("Step 5: Testing bit writes...\n");
    for (int i = 0; i < 19; i++) {
        mzd_write_bit(R1, 0, i, 0);
    }
    printf("SUCCESS: R1 bits written\n");
    
    for (int i = 0; i < 22; i++) {
        mzd_write_bit(R2, 0, i, 0);
    }
    printf("SUCCESS: R2 bits written\n");
    
    for (int i = 0; i < 23; i++) {
        mzd_write_bit(R3, 0, i, 0);
    }
    printf("SUCCESS: R3 bits written\n");
    
    for (int i = 0; i < 17; i++) {
        mzd_write_bit(R4, 0, i, 0);
    }
    printf("SUCCESS: R4 bits written\n");
    
    printf("Step 6: Cleanup...\n");
    mzd_free(R1);
    mzd_free(R2);
    mzd_free(R3);
    mzd_free(R4);
    printf("SUCCESS: All matrices freed\n");
    
    printf("All tests passed!\n");
    return 0;
} 
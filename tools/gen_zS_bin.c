#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_STATES 14  // 첫 줄 제외하고 14개 state

int main() {
    FILE* fin = fopen("textfiles/zS.txt", "r");
    if (!fin) {
        fprintf(stderr, "Failed to open textfiles/zS.txt\n");
        return 1;
    }
    
    FILE* fout = fopen("data/zS.bin", "wb");
    if (!fout) {
        fprintf(stderr, "Failed to open data/zS.bin for writing\n");
        fclose(fin);
        return 1;
    }
    
    char line[256];
    int state_count = 0;
    
    // 첫 줄 건너뛰기 (all zeros)
    fgets(line, sizeof(line), fin);
    
    // 나머지 14개 state 처리
    while (fgets(line, sizeof(line), fin) && state_count < NUM_STATES) {
        // R1(18비트) R2(21비트) R3(22비트) R4(16비트) 순서로 읽기
        int r1_bits[18], r2_bits[21], r3_bits[22], r4_bits[16];
        
        // R1 읽기 (18비트)
        for (int i = 0; i < 18; i++) {
            r1_bits[i] = line[i] - '0';
        }
        
        // R2 읽기 (21비트)
        for (int i = 0; i < 21; i++) {
            r2_bits[i] = line[19 + i] - '0';  // 공백 건너뛰고
        }
        
        // R3 읽기 (22비트)
        for (int i = 0; i < 22; i++) {
            r3_bits[i] = line[41 + i] - '0';  // 공백 건너뛰고
        }
        
        // R4 읽기 (16비트)
        for (int i = 0; i < 16; i++) {
            r4_bits[i] = line[64 + i] - '0';  // 공백 건너뛰고
        }
        
        // 바이너리로 저장 (비트 단위로)
        for (int i = 0; i < 18; i++) {
            fputc(r1_bits[i], fout);
        }
        for (int i = 0; i < 21; i++) {
            fputc(r2_bits[i], fout);
        }
        for (int i = 0; i < 22; i++) {
            fputc(r3_bits[i], fout);
        }
        for (int i = 0; i < 16; i++) {
            fputc(r4_bits[i], fout);
        }
        
        state_count++;
    }
    
    fclose(fin);
    fclose(fout);
    
    printf("zS.bin created successfully with %d states\n", state_count);
    printf("File size: %d bits (%d bytes)\n", state_count * 77, (state_count * 77 + 7) / 8);
    
    return 0;
} 
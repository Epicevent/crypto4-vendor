#include "../include/crypto_lib.h"

int main(void) {
    printf("=== Crypto Library Usage Example ===\n\n");
    
    // Start performance monitoring
    crypto_performance_start();
    
    // Example 1: Create and manipulate matrices
    printf("1. Matrix Operations:\n");
    
    crypto_matrix_t* A = crypto_matrix_init(4, 4, "Matrix A");
    crypto_matrix_t* B = crypto_matrix_init(4, 4, "Matrix B");
    
    // Fill matrices with some data
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            mzd_write_bit(A->data, i, j, (i + j) % 2);
            mzd_write_bit(B->data, i, j, (i * j) % 2);
        }
    }
    
    printf("Matrix A:\n");
    crypto_matrix_print_data(A);
    
    printf("Matrix B:\n");
    crypto_matrix_print_data(B);
    
    // Matrix operations
    crypto_matrix_t* C = crypto_matrix_multiply(A, B);
    printf("A * B:\n");
    crypto_matrix_print_data(C);
    
    crypto_matrix_t* D = crypto_matrix_add(A, B);
    printf("A + B:\n");
    crypto_matrix_print_data(D);
    
    // Get matrix statistics
    matrix_stats_t stats = crypto_matrix_get_stats(A);
    printf("Matrix A Statistics:\n");
    printf("  Size: %dx%d\n", stats.rows, stats.cols);
    printf("  Rank: %d\n", stats.rank);
    printf("  Invertible: %s\n", stats.is_invertible ? "Yes" : "No");
    printf("  Density: %.2f%%\n", stats.density * 100);
    printf("  Hamming Weight: %d\n", stats.hamming_weight);
    
    // Example 2: S-box analysis
    printf("\n2. S-box Analysis:\n");
    
    // Example 4-bit S-box (AES-like)
    int sbox[16] = {0xE, 0x4, 0xD, 0x1, 0x2, 0xF, 0xB, 0x8, 
                    0x3, 0xA, 0x6, 0xC, 0x5, 0x9, 0x0, 0x7};
    
    sbox_analysis_t* analysis = crypto_sbox_analysis(sbox, 4, 4);
    if (analysis) {
        printf("S-box Analysis Results:\n");
        printf("  Input Size: %d bits\n", analysis->input_size);
        printf("  Output Size: %d bits\n", analysis->output_size);
        printf("  Max Linear Bias: %.4f\n", analysis->lat->max_bias);
        printf("  Max Bias Input Mask: 0x%X\n", analysis->lat->max_input_mask);
        printf("  Max Bias Output Mask: 0x%X\n", analysis->lat->max_output_mask);
        
        printf("Linear Approximation Table (first 8x8):\n");
        for (int i = 0; i < 8; i++) {
            printf("  ");
            for (int j = 0; j < 8; j++) {
                printf("%d ", mzd_read_bit(analysis->lat->lat->data, i, j));
            }
            printf("\n");
        }
        
        crypto_sbox_analysis_free(analysis);
    }
    
    // Example 3: File I/O
    printf("\n3. File I/O Operations:\n");
    
    // Save matrix to file
    int save_result = crypto_matrix_save_text(A, "data/example_matrix.txt");
    if (save_result == CRYPTO_SUCCESS) {
        printf("Matrix saved to data/example_matrix.txt\n");
    }
    
    // Load matrix from file
    crypto_matrix_t* loaded = crypto_matrix_load_text("data/example_matrix.txt");
    if (loaded) {
        printf("Matrix loaded from file:\n");
        crypto_matrix_print_data(loaded);
        crypto_matrix_free(loaded);
    }
    
    // Example 4: Archive Operations
    printf("\n4. Archive Operations:\n");
    
    // Generate a larger archive for testing
    const char* test_archive = "data/test_archive.arc";
    int gen_result = crypto_archive_generate(test_archive, 10, 16, 16);
    if (gen_result == CRYPTO_SUCCESS) {
        printf("Archive generated: %s\n", test_archive);
        
        // List archive contents
        crypto_archive_list(test_archive);
        
        // Load a matrix from archive
        crypto_matrix_t* archived = crypto_matrix_load_archive(test_archive, 2);
        if (archived) {
            printf("\nLoaded matrix 2 from archive:\n");
            crypto_matrix_print_data(archived);
            crypto_matrix_free(archived);
        }
    }
    
    // Clean up
    crypto_matrix_free(A);
    crypto_matrix_free(B);
    crypto_matrix_free(C);
    crypto_matrix_free(D);
    
    // Performance results
    crypto_performance_t perf = crypto_performance_end();
    printf("\n5. Performance Results:\n");
    crypto_performance_print(&perf);
    
    printf("\n=== Example Completed ===\n");
    return 0;
} 
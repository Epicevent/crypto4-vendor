#ifndef CRYPTO_LIB_H
#define CRYPTO_LIB_H

#include <m4ri/m4ri.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "encrypt.h"

// ============================================================================
// CORE MATRIX OPERATIONS
// ============================================================================

/**
 * Matrix structure for cryptographic operations
 */
typedef struct {
    mzd_t* data;           // M4RI matrix
    int id;                 // Matrix identifier
    char* name;             // Matrix name/description
    int is_binary;          // Flag for binary matrices
} crypto_matrix_t;

/**
 * Initialize a cryptographic matrix
 */
crypto_matrix_t* crypto_matrix_init(int rows, int cols, const char* name);

/**
 * Free a cryptographic matrix
 */
void crypto_matrix_free(crypto_matrix_t* matrix);

/**
 * Create a copy of a matrix
 */
crypto_matrix_t* crypto_matrix_copy(const crypto_matrix_t* src);

/**
 * Generate random binary matrix
 */
crypto_matrix_t* crypto_matrix_random(int rows, int cols, const char* name);

// ============================================================================
// FILE I/O OPERATIONS
// ============================================================================

/**
 * Load matrix from text file
 */
crypto_matrix_t* crypto_matrix_load_text(const char* filename);

/**
 * Save matrix to text file
 */
int crypto_matrix_save_text(const crypto_matrix_t* matrix, const char* filename);

/**
 * Load matrix from archive file
 */
crypto_matrix_t* crypto_matrix_load_archive(const char* filename, int matrix_id);

/**
 * Save matrix to archive file
 */
int crypto_matrix_save_archive(const crypto_matrix_t* matrix, const char* filename, int matrix_id);

// ============================================================================
// ARCHIVE OPERATIONS
// ============================================================================

/**
 * Archive entry structure for M4RI matrices
 */
typedef struct {
    int matrix_id;
    int rows;
    int cols;
    size_t data_size;  // Size of matrix data in bytes
    // Matrix data follows this structure
} archive_entry_t;

/**
 * Archive information structure
 */
typedef struct {
    char* filename;
    int total_matrices;
    int matrix_rows;
    int matrix_cols;
    size_t file_size;
} archive_info_t;

/**
 * Get archive information
 */
archive_info_t* crypto_archive_get_info(const char* filename);

/**
 * Free archive information
 */
void crypto_archive_free_info(archive_info_t* info);

/**
 * Generate archive with multiple random matrices
 */
int crypto_archive_generate(const char* filename, int num_matrices, int rows, int cols);

/**
 * List all matrices in archive
 */
int crypto_archive_list(const char* filename);



// ============================================================================
// MATRIX OPERATIONS FOR CRYPTOGRAPHY
// ============================================================================

/**
 * Matrix multiplication in GF(2)
 */
crypto_matrix_t* crypto_matrix_multiply(const crypto_matrix_t* A, const crypto_matrix_t* B);

/**
 * Matrix addition in GF(2) (XOR)
 */
crypto_matrix_t* crypto_matrix_add(const crypto_matrix_t* A, const crypto_matrix_t* B);

/**
 * Matrix transpose
 */
crypto_matrix_t* crypto_matrix_transpose(const crypto_matrix_t* matrix);

/**
 * Calculate matrix rank
 */
int crypto_matrix_rank(const crypto_matrix_t* matrix);

/**
 * Check if matrix is invertible
 */
int crypto_matrix_is_invertible(const crypto_matrix_t* matrix);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Print matrix information
 */
void crypto_matrix_print(const crypto_matrix_t* matrix);

/**
 * Print matrix data
 */
void crypto_matrix_print_data(const crypto_matrix_t* matrix);

/**
 * Convert matrix to string representation
 */
char* crypto_matrix_to_string(const crypto_matrix_t* matrix);

/**
 * Get matrix statistics
 */
typedef struct {
    int rows;
    int cols;
    int rank;
    int is_invertible;
    double density;          // Percentage of non-zero elements
    int hamming_weight;      // Number of non-zero elements
} matrix_stats_t;

matrix_stats_t crypto_matrix_get_stats(const crypto_matrix_t* matrix);

// ============================================================================
// ERROR HANDLING
// ============================================================================

typedef enum {
    CRYPTO_SUCCESS = 0,
    CRYPTO_ERROR_MEMORY,
    CRYPTO_ERROR_FILE,
    CRYPTO_ERROR_INVALID_INPUT,
    CRYPTO_ERROR_DIMENSION_MISMATCH,
    CRYPTO_ERROR_NOT_IMPLEMENTED
} crypto_error_t;

/**
 * Get error message
 */
const char* crypto_error_message(crypto_error_t error);

/**
 * Set error handler
 */
typedef void (*crypto_error_handler_t)(crypto_error_t error, const char* message);
void crypto_set_error_handler(crypto_error_handler_t handler);

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

typedef struct {
    double cpu_time;
    size_t memory_used;
    int operations_count;
} crypto_performance_t;

/**
 * Start performance monitoring
 */
void crypto_performance_start(void);

/**
 * End performance monitoring and get results
 */
crypto_performance_t crypto_performance_end(void);

/**
 * Print performance results
 */
void crypto_performance_print(const crypto_performance_t* perf);

// m4ri 기반 평문 인코딩 함수 (plaintext -> e 벡터)
void encode_plaintext_m4ri(const char* plaintext, int* e, int num_blocks, const int* Gt);

// my_encrypt 기반 평문 인코딩 함수 (plaintext -> e 벡터)
void encoding_part_myenc(const char* plaintext, int* e_all, int num_blocks, const int* Gt);

// m4ri 기반 평문 인코딩 파트 (plaintext -> e_all)
void encoding_part_m4ri(const char* plaintext, int* e_all, int num_blocks, const int* Gt);

// m4ri 기반: LFSR 초기상태부터 모든 블록 암호화 (my_encrypt_from_state_precise와 동일 시그니처)
void encrypt_from_state_precise_m4ri(
    int R1_init, int R2_init, int R3_init, int R4_init,
    const char* plaintext, int err1, int err2, int err1_bit, int err2_bit, int* c_out,
    const int* s, const int* Gt, int* z_out
);

#endif // CRYPTO_LIB_H 
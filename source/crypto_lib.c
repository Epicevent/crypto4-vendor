#include "crypto_lib.h"
#include <string.h>
#include <math.h>
#include <m4ri/m4ri.h>

// Fallback for __builtin_parity if not available
#ifndef __builtin_parity
static int builtin_parity(unsigned int x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count % 2;
}
#define __builtin_parity builtin_parity
#endif

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

static crypto_error_handler_t g_error_handler = NULL;
static clock_t g_performance_start = 0;
static size_t g_memory_used = 0;
static int g_operations_count = 0;

// ============================================================================
// ERROR HANDLING
// ============================================================================

const char* crypto_error_message(crypto_error_t error) {
    switch (error) {
        case CRYPTO_SUCCESS: return "Success";
        case CRYPTO_ERROR_MEMORY: return "Memory allocation failed";
        case CRYPTO_ERROR_FILE: return "File operation failed";
        case CRYPTO_ERROR_INVALID_INPUT: return "Invalid input parameters";
        case CRYPTO_ERROR_DIMENSION_MISMATCH: return "Matrix dimension mismatch";
        case CRYPTO_ERROR_NOT_IMPLEMENTED: return "Function not implemented";
        default: return "Unknown error";
    }
}

void crypto_set_error_handler(crypto_error_handler_t handler) {
    g_error_handler = handler;
}

static void crypto_raise_error(crypto_error_t error, const char* message) {
    if (g_error_handler) {
        g_error_handler(error, message);
    } else {
        fprintf(stderr, "Crypto Error: %s - %s\n", crypto_error_message(error), message);
    }
}

// ============================================================================
// CORE MATRIX OPERATIONS
// ============================================================================

crypto_matrix_t* crypto_matrix_init(int rows, int cols, const char* name) {
    if (rows <= 0 || cols <= 0) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid matrix dimensions");
        return NULL;
    }
    
    crypto_matrix_t* matrix = (crypto_matrix_t*)malloc(sizeof(crypto_matrix_t));
    if (!matrix) {
        crypto_raise_error(CRYPTO_ERROR_MEMORY, "Failed to allocate matrix structure");
        return NULL;
    }
    
    matrix->data = mzd_init(rows, cols);
    if (!matrix->data) {
        free(matrix);
        crypto_raise_error(CRYPTO_ERROR_MEMORY, "Failed to allocate M4RI matrix");
        return NULL;
    }
    
    matrix->id = -1;  // Will be set by caller if needed
    matrix->is_binary = 1;  // Default to binary
    
    if (name) {
        matrix->name = strdup(name);
    } else {
        matrix->name = strdup("unnamed");
    }
    
    g_memory_used += sizeof(crypto_matrix_t) + rows * cols / 8;  // Approximate
    return matrix;
}

void crypto_matrix_free(crypto_matrix_t* matrix) {
    if (!matrix) return;
    
    if (matrix->data) {
        mzd_free(matrix->data);
    }
    
    if (matrix->name) {
        free(matrix->name);
    }
    
    g_memory_used -= sizeof(crypto_matrix_t) + matrix->data->nrows * matrix->data->ncols / 8;
    free(matrix);
}

crypto_matrix_t* crypto_matrix_copy(const crypto_matrix_t* src) {
    if (!src || !src->data) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid source matrix");
        return NULL;
    }
    
    crypto_matrix_t* dest = crypto_matrix_init(src->data->nrows, src->data->ncols, src->name);
    if (!dest) return NULL;
    
    mzd_copy(dest->data, src->data);
    dest->id = src->id;
    dest->is_binary = src->is_binary;
    
    return dest;
}

crypto_matrix_t* crypto_matrix_random(int rows, int cols, const char* name) {
    crypto_matrix_t* matrix = crypto_matrix_init(rows, cols, name);
    if (!matrix) return NULL;
    
    mzd_randomize(matrix->data);
    return matrix;
}

// ============================================================================
// FILE I/O OPERATIONS
// ============================================================================

crypto_matrix_t* crypto_matrix_load_text(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot open file for reading");
        return NULL;
    }
    
    int rows, cols;
    if (fscanf(file, "%d %d", &rows, &cols) != 2) {
        fclose(file);
        crypto_raise_error(CRYPTO_ERROR_FILE, "Invalid file format");
        return NULL;
    }
    
    crypto_matrix_t* matrix = crypto_matrix_init(rows, cols, filename);
    if (!matrix) {
        fclose(file);
        return NULL;
    }
    
    // Read matrix data
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int bit;
            if (fscanf(file, "%d", &bit) != 1) {
                crypto_matrix_free(matrix);
                fclose(file);
                crypto_raise_error(CRYPTO_ERROR_FILE, "Invalid matrix data");
                return NULL;
            }
            mzd_write_bit(matrix->data, i, j, bit);
        }
    }
    
    fclose(file);
    return matrix;
}

int crypto_matrix_save_text(const crypto_matrix_t* matrix, const char* filename) {
    if (!matrix || !matrix->data) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid matrix");
        return CRYPTO_ERROR_INVALID_INPUT;
    }
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot open file for writing");
        return CRYPTO_ERROR_FILE;
    }
    
    // Write dimensions
    fprintf(file, "%d %d\n", matrix->data->nrows, matrix->data->ncols);
    
    // Write matrix data
    for (int i = 0; i < matrix->data->nrows; i++) {
        for (int j = 0; j < matrix->data->ncols; j++) {
            fprintf(file, "%d", mzd_read_bit(matrix->data, i, j));
            if (j < matrix->data->ncols - 1) fprintf(file, " ");
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    return CRYPTO_SUCCESS;
}

// Archive operations (simplified version)
crypto_matrix_t* crypto_matrix_load_archive(const char* filename, int matrix_id) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot open archive file");
        return NULL;
    }
    
    // Find the requested matrix by scanning through entries
    long current_offset = 0;
    archive_entry_t entry;
    
    for (int i = 0; i <= matrix_id; i++) {
        if (fseek(file, current_offset, SEEK_SET) != 0) {
            fclose(file);
            crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot seek in archive file");
            return NULL;
        }
        
        // Read entry header
        if (fread(&entry, sizeof(archive_entry_t), 1, file) != 1) {
            fclose(file);
            crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot read archive entry");
            return NULL;
        }
        
        if (i == matrix_id) {
            // Found the requested matrix
            crypto_matrix_t* matrix = crypto_matrix_init(entry.rows, entry.cols, "archive_matrix");
            if (!matrix) {
                fclose(file);
                crypto_raise_error(CRYPTO_ERROR_MEMORY, "Failed to allocate matrix from archive");
                return NULL;
            }
            
            matrix->id = entry.matrix_id;
            
            // Read matrix data as bit array
            unsigned char* buffer = (unsigned char*)malloc(entry.data_size);
            if (!buffer) {
                crypto_matrix_free(matrix);
                fclose(file);
                crypto_raise_error(CRYPTO_ERROR_MEMORY, "Cannot allocate buffer for matrix data");
                return NULL;
            }
            
            if (fread(buffer, entry.data_size, 1, file) != 1) {
                free(buffer);
                crypto_matrix_free(matrix);
                fclose(file);
                crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot read matrix data");
                return NULL;
            }
            
            // Convert bit array to matrix
            for (int r = 0; r < entry.rows; r++) {
                for (int c = 0; c < entry.cols; c++) {
                    int bit_pos = r * entry.cols + c;
                    int byte_pos = bit_pos / 8;
                    int bit_offset = bit_pos % 8;
                    
                    int bit = (buffer[byte_pos] >> bit_offset) & 1;
                    mzd_write_bit(matrix->data, r, c, bit);
                }
            }
            
            free(buffer);
            fclose(file);
            return matrix;
        }
        
        // Skip to next entry
        current_offset += sizeof(archive_entry_t) + entry.data_size;
    }
    
    fclose(file);
    crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Matrix ID not found in archive");
    return NULL;
}

int crypto_matrix_save_archive(const crypto_matrix_t* matrix, const char* filename, int matrix_id) {
    if (!matrix || !matrix->data) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid matrix for archive save");
        return CRYPTO_ERROR_INVALID_INPUT;
    }
    
    FILE* file = fopen(filename, "ab");  // Append mode
    if (!file) {
        crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot create/open archive file");
        return CRYPTO_ERROR_FILE;
    }
    
    // Calculate matrix data size (bit-based)
    size_t matrix_data_size = (matrix->data->nrows * matrix->data->ncols + 7) / 8;
    
    // Create archive entry
    archive_entry_t entry;
    entry.matrix_id = matrix_id;
    entry.rows = matrix->data->nrows;
    entry.cols = matrix->data->ncols;
    entry.data_size = matrix_data_size;
    
    // Write entry header
    if (fwrite(&entry, sizeof(archive_entry_t), 1, file) != 1) {
        fclose(file);
        crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot write archive entry header");
        return CRYPTO_ERROR_FILE;
    }
    
    // Write matrix data as bit array
    unsigned char* buffer = (unsigned char*)malloc(matrix_data_size);
    if (!buffer) {
        fclose(file);
        crypto_raise_error(CRYPTO_ERROR_MEMORY, "Cannot allocate buffer for matrix data");
        return CRYPTO_ERROR_MEMORY;
    }
    
    memset(buffer, 0, matrix_data_size);
    
    for (int r = 0; r < matrix->data->nrows; r++) {
        for (int c = 0; c < matrix->data->ncols; c++) {
            int bit_pos = r * matrix->data->ncols + c;
            int byte_pos = bit_pos / 8;
            int bit_offset = bit_pos % 8;
            
            if (mzd_read_bit(matrix->data, r, c)) {
                buffer[byte_pos] |= (1 << bit_offset);
            }
        }
    }
    
    if (fwrite(buffer, matrix_data_size, 1, file) != 1) {
        free(buffer);
        fclose(file);
        crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot write matrix data");
        return CRYPTO_ERROR_FILE;
    }
    
    free(buffer);
    fclose(file);
    return CRYPTO_SUCCESS;
}

// ============================================================================
// ARCHIVE OPERATIONS
// ============================================================================

archive_info_t* crypto_archive_get_info(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot open archive file");
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size == 0) {
        fclose(file);
        crypto_raise_error(CRYPTO_ERROR_FILE, "Archive file is empty");
        return NULL;
    }
    
    // Count matrices by scanning through entries
    int total_matrices = 0;
    long current_offset = 0;
    archive_entry_t entry;
    
    while (current_offset < file_size) {
        if (fseek(file, current_offset, SEEK_SET) != 0) {
            break;
        }
        
        if (fread(&entry, sizeof(archive_entry_t), 1, file) != 1) {
            break;
        }
        
        total_matrices++;
        current_offset += sizeof(archive_entry_t) + entry.data_size;
    }
    
    // Read first entry to get matrix dimensions
    fseek(file, 0, SEEK_SET);
    archive_entry_t first_entry;
    if (fread(&first_entry, sizeof(archive_entry_t), 1, file) != 1) {
        fclose(file);
        crypto_raise_error(CRYPTO_ERROR_FILE, "Cannot read archive header");
        return NULL;
    }
    
    fclose(file);
    
    // Create info structure
    archive_info_t* info = (archive_info_t*)malloc(sizeof(archive_info_t));
    if (!info) {
        crypto_raise_error(CRYPTO_ERROR_MEMORY, "Failed to allocate archive info");
        return NULL;
    }
    
    info->filename = strdup(filename);
    info->total_matrices = total_matrices;
    info->matrix_rows = first_entry.rows;
    info->matrix_cols = first_entry.cols;
    info->file_size = file_size;
    
    return info;
}

void crypto_archive_free_info(archive_info_t* info) {
    if (!info) return;
    
    if (info->filename) {
        free(info->filename);
    }
    
    free(info);
}

int crypto_archive_generate(const char* filename, int num_matrices, int rows, int cols) {
    if (num_matrices <= 0 || rows <= 0 || cols <= 0) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid archive generation parameters");
        return CRYPTO_ERROR_INVALID_INPUT;
    }
    
    // Remove size limitation - M4RI can handle large matrices
    if (rows > 10000 || cols > 10000) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Matrix dimensions too large");
        return CRYPTO_ERROR_INVALID_INPUT;
    }
    
    // Remove existing file if it exists
    remove(filename);
    
    srand(time(NULL));
    
    for (int i = 0; i < num_matrices; i++) {
        // Create random matrix
        crypto_matrix_t* matrix = crypto_matrix_random(rows, cols, "archive_matrix");
        if (!matrix) {
            crypto_raise_error(CRYPTO_ERROR_MEMORY, "Failed to create random matrix");
            return CRYPTO_ERROR_MEMORY;
        }
        
        // Save to archive
        int result = crypto_matrix_save_archive(matrix, filename, i);
        crypto_matrix_free(matrix);
        
        if (result != CRYPTO_SUCCESS) {
            return result;
        }
        
        // Progress indicator for large archives
        if (i % 1000 == 0 && i > 0) {
            printf("Generated %d matrices...\n", i);
        }
    }
    
    return CRYPTO_SUCCESS;
}

int crypto_archive_list(const char* filename) {
    archive_info_t* info = crypto_archive_get_info(filename);
    if (!info) {
        return CRYPTO_ERROR_FILE;
    }
    
    printf("Archive: %s\n", info->filename);
    printf("Total matrices: %d\n", info->total_matrices);
    printf("Matrix dimensions: %dx%d\n", info->matrix_rows, info->matrix_cols);
    printf("File size: %zu bytes\n", info->file_size);
    printf("\nMatrix list:\n");
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        crypto_archive_free_info(info);
        return CRYPTO_ERROR_FILE;
    }
    
    long current_offset = 0;
    for (int i = 0; i < info->total_matrices && i < 10; i++) {  // Show first 10
        archive_entry_t entry;
        
        if (fseek(file, current_offset, SEEK_SET) == 0 && 
            fread(&entry, sizeof(archive_entry_t), 1, file) == 1) {
            printf("  Matrix %d: %dx%d (size: %zu bytes)\n", 
                   entry.matrix_id, entry.rows, entry.cols, entry.data_size);
            current_offset += sizeof(archive_entry_t) + entry.data_size;
        } else {
            break;
        }
    }
    
    if (info->total_matrices > 10) {
        printf("  ... and %d more matrices\n", info->total_matrices - 10);
    }
    
    fclose(file);
    crypto_archive_free_info(info);
    return CRYPTO_SUCCESS;
}

// ============================================================================
// MATRIX OPERATIONS FOR CRYPTOGRAPHY
// ============================================================================

crypto_matrix_t* crypto_matrix_multiply(const crypto_matrix_t* A, const crypto_matrix_t* B) {
    if (!A || !B || !A->data || !B->data) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid matrices for multiplication");
        return NULL;
    }
    
    if (A->data->ncols != B->data->nrows) {
        crypto_raise_error(CRYPTO_ERROR_DIMENSION_MISMATCH, "Matrix dimensions don't match for multiplication");
        return NULL;
    }
    
    crypto_matrix_t* result = crypto_matrix_init(A->data->nrows, B->data->ncols, "multiply_result");
    if (!result) return NULL;
    
    mzd_mul(result->data, A->data, B->data, 0);
    g_operations_count++;
    
    return result;
}

crypto_matrix_t* crypto_matrix_add(const crypto_matrix_t* A, const crypto_matrix_t* B) {
    if (!A || !B || !A->data || !B->data) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid matrices for addition");
        return NULL;
    }
    
    if (A->data->nrows != B->data->nrows || A->data->ncols != B->data->ncols) {
        crypto_raise_error(CRYPTO_ERROR_DIMENSION_MISMATCH, "Matrix dimensions don't match for addition");
        return NULL;
    }
    
    crypto_matrix_t* result = crypto_matrix_copy(A);
    if (!result) return NULL;
    
    mzd_add(result->data, result->data, B->data);
    g_operations_count++;
    
    return result;
}

crypto_matrix_t* crypto_matrix_transpose(const crypto_matrix_t* matrix) {
    if (!matrix || !matrix->data) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid matrix for transpose");
        return NULL;
    }
    
    crypto_matrix_t* result = crypto_matrix_init(matrix->data->ncols, matrix->data->nrows, "transpose_result");
    if (!result) return NULL;
    
    mzd_t* transposed = mzd_transpose(NULL, matrix->data);
    mzd_copy(result->data, transposed);
    mzd_free(transposed);
    
    return result;
}

int crypto_matrix_rank(const crypto_matrix_t* matrix) {
    if (!matrix || !matrix->data) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid matrix for rank calculation");
        return -1;
    }
    
    mzd_t* temp = mzd_copy(NULL, matrix->data);
    int rank = mzd_echelonize(temp, 0);
    mzd_free(temp);
    
    return rank;
}

int crypto_matrix_is_invertible(const crypto_matrix_t* matrix) {
    if (!matrix || !matrix->data) {
        return 0;
    }
    
    if (matrix->data->nrows != matrix->data->ncols) {
        return 0;  // Non-square matrices are not invertible
    }
    
    int rank = crypto_matrix_rank(matrix);
    return (rank == matrix->data->nrows);
}

// ============================================================================
// CRYPTOGRAPHIC ANALYSIS
// ============================================================================

linear_analysis_t* crypto_linear_analysis(int* sbox, int input_size, int output_size) {
    if (!sbox || input_size <= 0 || output_size <= 0) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid S-box parameters");
        return NULL;
    }
    
    linear_analysis_t* analysis = (linear_analysis_t*)malloc(sizeof(linear_analysis_t));
    if (!analysis) {
        crypto_raise_error(CRYPTO_ERROR_MEMORY, "Failed to allocate analysis structure");
        return NULL;
    }
    
    analysis->input_size = input_size;
    analysis->output_size = output_size;
    analysis->max_bias = 0.0;
    analysis->max_input_mask = 0;
    analysis->max_output_mask = 0;
    
    int input_max = 1 << input_size;
    int output_max = 1 << output_size;
    
    // Create LAT matrix
    analysis->lat = crypto_matrix_init(input_max, output_max, "LAT");
    if (!analysis->lat) {
        free(analysis);
        return NULL;
    }
    
    // Calculate Linear Approximation Table
    for (int input_mask = 0; input_mask < input_max; input_mask++) {
        for (int output_mask = 0; output_mask < output_max; output_mask++) {
            int bias = 0;
            
            for (int input = 0; input < input_max; input++) {
                int output = sbox[input];
                
                // Calculate linear approximation
                int input_parity = __builtin_parity(input & input_mask);
                int output_parity = __builtin_parity(output & output_mask);
                
                if (input_parity == output_parity) {
                    bias++;
                } else {
                    bias--;
                }
            }
            
            // Store bias in matrix
            mzd_write_bit(analysis->lat->data, input_mask, output_mask, (bias > 0) ? 1 : 0);
            
            // Track maximum bias
            double abs_bias = fabs((double)bias / input_max);
            if (abs_bias > analysis->max_bias) {
                analysis->max_bias = abs_bias;
                analysis->max_input_mask = input_mask;
                analysis->max_output_mask = output_mask;
            }
        }
    }
    
    return analysis;
}

void crypto_linear_analysis_free(linear_analysis_t* analysis) {
    if (!analysis) return;
    
    if (analysis->lat) {
        crypto_matrix_free(analysis->lat);
    }
    
    free(analysis);
}

sbox_analysis_t* crypto_sbox_analysis(int* sbox, int input_size, int output_size) {
    if (!sbox || input_size <= 0 || output_size <= 0) {
        crypto_raise_error(CRYPTO_ERROR_INVALID_INPUT, "Invalid S-box parameters");
        return NULL;
    }
    
    sbox_analysis_t* analysis = (sbox_analysis_t*)malloc(sizeof(sbox_analysis_t));
    if (!analysis) {
        crypto_raise_error(CRYPTO_ERROR_MEMORY, "Failed to allocate S-box analysis structure");
        return NULL;
    }
    
    analysis->input_size = input_size;
    analysis->output_size = output_size;
    
    int sbox_size = 1 << input_size;
    analysis->sbox = (int*)malloc(sbox_size * sizeof(int));
    if (!analysis->sbox) {
        free(analysis);
        crypto_raise_error(CRYPTO_ERROR_MEMORY, "Failed to allocate S-box data");
        return NULL;
    }
    
    // Copy S-box data
    memcpy(analysis->sbox, sbox, sbox_size * sizeof(int));
    
    // Perform linear analysis
    analysis->lat = crypto_linear_analysis(sbox, input_size, output_size);
    if (!analysis->lat) {
        free(analysis->sbox);
        free(analysis);
        return NULL;
    }
    
    // TODO: Implement DDT calculation
    analysis->ddt = NULL;
    
    return analysis;
}

void crypto_sbox_analysis_free(sbox_analysis_t* analysis) {
    if (!analysis) return;
    
    if (analysis->sbox) {
        free(analysis->sbox);
    }
    
    if (analysis->lat) {
        crypto_linear_analysis_free(analysis->lat);
    }
    
    if (analysis->ddt) {
        crypto_matrix_free(analysis->ddt);
    }
    
    free(analysis);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void crypto_matrix_print(const crypto_matrix_t* matrix) {
    if (!matrix || !matrix->data) {
        printf("Invalid matrix\n");
        return;
    }
    
    printf("Matrix: %s (%dx%d)\n", matrix->name, matrix->data->nrows, matrix->data->ncols);
    printf("ID: %d, Binary: %s\n", matrix->id, matrix->is_binary ? "Yes" : "No");
}

void crypto_matrix_print_data(const crypto_matrix_t* matrix) {
    if (!matrix || !matrix->data) {
        printf("Invalid matrix\n");
        return;
    }
    
    crypto_matrix_print(matrix);
    printf("Data:\n");
    
    for (int i = 0; i < matrix->data->nrows; i++) {
        printf("  ");
        for (int j = 0; j < matrix->data->ncols; j++) {
            printf("%d ", mzd_read_bit(matrix->data, i, j));
        }
        printf("\n");
    }
    printf("\n");
}

char* crypto_matrix_to_string(const crypto_matrix_t* matrix) {
    if (!matrix || !matrix->data) {
        return strdup("Invalid matrix");
    }
    
    // Calculate required size
    int size = 256 + matrix->data->nrows * matrix->data->ncols * 3;  // Approximate
    char* result = (char*)malloc(size);
    if (!result) return NULL;
    
    int pos = snprintf(result, size, "Matrix: %s (%dx%d)\n", 
                       matrix->name, matrix->data->nrows, matrix->data->ncols);
    
    for (int i = 0; i < matrix->data->nrows && pos < size - 10; i++) {
        pos += snprintf(result + pos, size - pos, "  ");
        for (int j = 0; j < matrix->data->ncols && pos < size - 5; j++) {
            pos += snprintf(result + pos, size - pos, "%d ", mzd_read_bit(matrix->data, i, j));
        }
        pos += snprintf(result + pos, size - pos, "\n");
    }
    
    return result;
}

matrix_stats_t crypto_matrix_get_stats(const crypto_matrix_t* matrix) {
    matrix_stats_t stats = {0};
    
    if (!matrix || !matrix->data) {
        return stats;
    }
    
    stats.rows = matrix->data->nrows;
    stats.cols = matrix->data->ncols;
    stats.rank = crypto_matrix_rank(matrix);
    stats.is_invertible = crypto_matrix_is_invertible(matrix);
    
    // Calculate density and hamming weight
    int total_elements = stats.rows * stats.cols;
    int non_zero = 0;
    
    for (int i = 0; i < stats.rows; i++) {
        for (int j = 0; j < stats.cols; j++) {
            if (mzd_read_bit(matrix->data, i, j)) {
                non_zero++;
            }
        }
    }
    
    stats.hamming_weight = non_zero;
    stats.density = (double)non_zero / total_elements;
    
    return stats;
}

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

void crypto_performance_start(void) {
    g_performance_start = clock();
    g_memory_used = 0;
    g_operations_count = 0;
}

crypto_performance_t crypto_performance_end(void) {
    crypto_performance_t perf;
    perf.cpu_time = ((double)(clock() - g_performance_start)) / CLOCKS_PER_SEC;
    perf.memory_used = g_memory_used;
    perf.operations_count = g_operations_count;
    return perf;
}

void crypto_performance_print(const crypto_performance_t* perf) {
    printf("Performance Results:\n");
    printf("  CPU Time: %.3f seconds\n", perf->cpu_time);
    printf("  Memory Used: %zu bytes\n", perf->memory_used);
    printf("  Operations Count: %d\n", perf->operations_count);
} 

// m4ri 기반 평문 인코딩 함수 (plaintext -> e 벡터)
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

// --- 신뢰 가능한 인코딩 파트: 평문 -> e_all ---
// (encrypt.c에서만 구현)
// ... existing code ... 
#ifndef MAINTENANCE_H
#define MAINTENANCE_H

// =============================================================================
//  Constants
// =============================================================================
#define CSV_FILE_DEFAULT "maintenance.csv"
#define CSV_PATH_MAX 512

#define MAX_RECORDS 10000
#define MAX_NAME 64
#define MAX_ID 32
#define MAX_DATE 16
#define MAX_DETAILS 256

// =============================================================================
//  Input Status Codes
// =============================================================================
#define INPUT_OK 0
#define INPUT_ERROR -1
#define INPUT_CANCELLED -2
#define INPUT_TOO_LONG -3
#define INPUT_EXIT -4
#define INPUT_BACK -5

// =============================================================================
//  Record Operation Results
// =============================================================================
typedef enum record_result
{
    RECORD_SUCCESS = 0,
    RECORD_ERROR_STORAGE_FULL = -1,
    RECORD_ERROR_INVALID_DATA = -2,
    RECORD_ERROR_DUPLICATE_ID = -3,
    RECORD_ERROR_NOT_FOUND = -4
} record_result_t;

#endif /* MAINTENANCE_H */

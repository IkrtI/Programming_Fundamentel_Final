#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define close _close
#define fileno _fileno
#endif

#include "../maintenance.h"
#include "maintenance_test_api.h"

#define MAX_RESULTS 128

typedef struct
{
    const char *suite;
    const char *name;
    int passed;
    char detail[160];
} test_result_t;

static test_result_t results[MAX_RESULTS];
static size_t result_count = 0;

static void record_result(const char *suite,
                          const char *name,
                          int passed,
                          const char *fmt,
                          ...)
{
    if (result_count >= MAX_RESULTS)
    {
        return;
    }

    test_result_t *entry = &results[result_count++];
    entry->suite = suite;
    entry->name = name;
    entry->passed = passed;

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->detail, sizeof(entry->detail), fmt, args);
    va_end(args);
}

#define ASSERT_TRUE(SUITE, NAME, EXPR, FMT, ...)                                                     \
    do                                                                                               \
    {                                                                                                \
        int _ok = (EXPR);                                                                            \
        record_result((SUITE), (NAME), _ok, (FMT), ##__VA_ARGS__);                                   \
    } while (0)

#define ASSERT_EQ_INT(SUITE, NAME, EXPECTED, ACTUAL)                                                 \
    do                                                                                               \
    {                                                                                                \
        int _exp = (EXPECTED);                                                                       \
        int _act = (ACTUAL);                                                                         \
        int _ok = (_exp == _act);                                                                    \
        record_result((SUITE), (NAME), _ok, "expected %d, actual %d", _exp, _act);                   \
    } while (0)

#define ASSERT_STREQ(SUITE, NAME, EXPECTED, ACTUAL)                                                  \
    do                                                                                               \
    {                                                                                                \
        const char *_exp = (EXPECTED);                                                               \
        const char *_act = (ACTUAL);                                                                 \
        int _ok = ((_exp == NULL && _act == NULL) ||                                                 \
                    (_exp != NULL && _act != NULL && strcmp(_exp, _act) == 0));                      \
        record_result((SUITE), (NAME), _ok, "expected '%s', actual '%s'",                            \
                       _exp ? _exp : "(null)", _act ? _act : "(null)");                           \
    } while (0)

static void print_results_table(void)
{
    size_t passed = 0;
    for (size_t i = 0; i < result_count; ++i)
    {
        const test_result_t *row = &results[i];
        if (row->passed)
        {
            ++passed;
        }
        printf("[%s] %s — %s %s\n",
               row->suite ? row->suite : "",
               row->name ? row->name : "",
               row->passed ? "✓" : "✗",
               row->detail);
    }

    printf("Total: %zu, Passed: %zu, Failed: %zu\n", result_count, passed, result_count - passed);
    puts("-------");
}

static void reset_storage(void)
{
    memset(machineName, 0, sizeof(machineName));
    memset(machineID, 0, sizeof(machineID));
    memset(maintenanceDate, 0, sizeof(maintenanceDate));
    memset(maintenanceDetails, 0, sizeof(maintenanceDetails));
    record_count = 0;
}

static int redirect_stdin_from_string(const char *text, int *saved_fd, FILE **temp_file)
{
    if (!text)
    {
        return -1;
    }

    *saved_fd = dup(fileno(stdin));
    if (*saved_fd < 0)
    {
        return -1;
    }

    *temp_file = tmpfile();
    if (!*temp_file)
    {
        close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    fputs(text, *temp_file);
    rewind(*temp_file);

    if (dup2(fileno(*temp_file), fileno(stdin)) != 0)
    {
        fclose(*temp_file);
        *temp_file = NULL;
        close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    return 0;
}

static void restore_stdin(int saved_fd, FILE *temp_file)
{
    if (saved_fd >= 0)
    {
        dup2(saved_fd, fileno(stdin));
        close(saved_fd);
    }

    if (temp_file)
    {
        fclose(temp_file);
    }

    clearerr(stdin);
}

static void test_csv_path_management(void)
{
    const char *suite = "Storage";
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/unit_path_primary.csv";
    remove(path);

    ASSERT_TRUE(suite, "set csv path valid", maintenance_set_csv_path(path) == 0,
                "set path returned %d", maintenance_set_csv_path(path));
    ASSERT_STREQ(suite, "get csv path", path, maintenance_get_csv_path());

    ASSERT_TRUE(suite, "reject null path", maintenance_set_csv_path(NULL) != 0,
                "NULL path should fail");

    ASSERT_TRUE(suite, "restore path", maintenance_set_csv_path(original) == 0,
                "restore returned %d", maintenance_set_csv_path(original));
}

static void test_ensure_csv_exists_blank(void)
{
    const char *suite = "Storage";
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/unit_blank.csv";
    remove(path);
    maintenance_set_csv_path(path);

    int saved_fd = -1;
    FILE *temp = NULL;
    ASSERT_TRUE(suite, "inject decline input",
                redirect_stdin_from_string("n\n", &saved_fd, &temp) == 0,
                "stdin redirected");

    int ensure_status = ensure_csv_exists();
    restore_stdin(saved_fd, temp);
    ASSERT_TRUE(suite, "ensure_csv_exists blank", ensure_status == 0,
                "ensure status %d", ensure_status);

    FILE *f = fopen(path, "r");
    ASSERT_TRUE(suite, "blank file created", f != NULL,
                "blank csv present");
    if (f)
    {
        char buffer[256] = {0};
        size_t read = fread(buffer, 1, sizeof(buffer) - 1, f);
        buffer[read] = '\0';
        fclose(f);
        ASSERT_STREQ(suite, "blank header",
                     "MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n",
                     buffer);
    }

    remove(path);
    maintenance_set_csv_path(original);
}

static void test_save_and_load_round_trip(void)
{
    const char *suite = "Storage";
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/unit_round_trip.csv";
    remove(path);
    maintenance_set_csv_path(path);

    reset_storage();
    snprintf(machineName[0], MAX_NAME, "%s", "Hydraulic Press");
    snprintf(machineID[0], MAX_ID, "%s", "HP-001");
    snprintf(maintenanceDate[0], MAX_DATE, "%s", "2025-08-01");
    snprintf(maintenanceDetails[0], MAX_DETAILS, "%s", "Changed hydraulic fluid");
    record_count = 1;

    ASSERT_TRUE(suite, "save_all_records", save_all_records() == 0,
                "save returned %d", save_all_records());

    reset_storage();
    ASSERT_TRUE(suite, "load_records", load_records() == 0,
                "load returned %d", load_records());
    ASSERT_EQ_INT(suite, "record count", 1, record_count);
    ASSERT_STREQ(suite, "machine name", "Hydraulic Press", machineName[0]);
    ASSERT_STREQ(suite, "machine id", "HP-001", machineID[0]);

    remove(path);
    maintenance_set_csv_path(original);
    reset_storage();
}

static void test_storage_capacity_guard(void)
{
    const char *suite = "Storage";
    reset_storage();
    record_count = MAX_RECORDS;
    ASSERT_TRUE(suite, "storage full", is_record_storage_full(),
                "expected storage to be full");
}

int main(void)
{
    test_csv_path_management();
    test_ensure_csv_exists_blank();
    test_save_and_load_round_trip();
    test_storage_capacity_guard();

    print_results_table();

    size_t failures = 0;
    for (size_t i = 0; i < result_count; ++i)
    {
        if (!results[i].passed)
        {
            ++failures;
        }
    }

    return failures == 0 ? 0 : 1;
}

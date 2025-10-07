#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

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

#define MAX_RESULTS 512

typedef struct
{
    const char *scenario;
    const char *step;
    int passed;
    char detail[256];
} scenario_result_t;

static scenario_result_t results[MAX_RESULTS];
static size_t result_count = 0;

static void record_result(const char *scenario,
                          const char *step,
                          int passed,
                          const char *fmt,
                          ...)
{
    if (result_count >= MAX_RESULTS)
    {
        return;
    }

    scenario_result_t *entry = &results[result_count++];
    entry->scenario = scenario;
    entry->step = step;
    entry->passed = passed;

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->detail, sizeof(entry->detail), fmt, args);
    va_end(args);
}

#define SCENARIO_ASSERT_TRUE(SC, STEP, EXPR, FMT, ...)                                              \
    do                                                                                              \
    {                                                                                               \
        int _ok = (EXPR);                                                                           \
        record_result((SC), (STEP), _ok, (FMT), ##__VA_ARGS__);                                     \
    } while (0)

#define SCENARIO_ASSERT_EQ_INT(SC, STEP, EXPECTED, ACTUAL)                                          \
    do                                                                                              \
    {                                                                                               \
        int _exp = (EXPECTED);                                                                      \
        int _act = (ACTUAL);                                                                        \
        int _ok = (_exp == _act);                                                                   \
        record_result((SC), (STEP), _ok, "expected %d, actual %d", _exp, _act);                     \
    } while (0)

#define SCENARIO_ASSERT_STREQ(SC, STEP, EXPECTED, ACTUAL)                                           \
    do                                                                                              \
    {                                                                                               \
        const char *_exp = (EXPECTED);                                                              \
        const char *_act = (ACTUAL);                                                                \
        int _ok = ((_exp == NULL && _act == NULL) ||                                                \
                    (_exp != NULL && _act != NULL && strcmp(_exp, _act) == 0));                     \
        record_result((SC), (STEP), _ok, "expected '%s', actual '%s'",                              \
                       _exp ? _exp : "(null)", _act ? _act : "(null)");                           \
    } while (0)

static void print_results_table(void)
{
    size_t passed = 0;
    for (size_t i = 0; i < result_count; ++i)
    {
        const scenario_result_t *row = &results[i];
        if (row->passed)
        {
            ++passed;
        }

        fprintf(stderr, "%s %s | %s | %s\n",
                row->passed ? "[PASS]" : "[FAIL]",
                row->scenario ? row->scenario : "",
                row->step ? row->step : "",
                row->detail);
    }

    fprintf(stderr, "Total: %zu, Passed: %zu, Failed: %zu\n",
            result_count, passed, result_count - passed);
    fprintf(stderr, "-------\n");
}

static void reset_storage(void)
{
    memset(machineName, 0, sizeof(machineName));
    memset(machineID, 0, sizeof(machineID));
    memset(maintenanceDate, 0, sizeof(maintenanceDate));
    memset(maintenanceDetails, 0, sizeof(maintenanceDetails));
    record_count = 0;
}

static void restore_stdin(int saved_fd, FILE *temp)
{
    if (saved_fd >= 0)
    {
        dup2(saved_fd, fileno(stdin));
        close(saved_fd);
    }

    if (temp)
    {
        fclose(temp);
    }

    clearerr(stdin);
}

static int feed_input(const char *script, FILE **out_temp)
{
    if (!script)
    {
        return -1;
    }

    int saved_fd = dup(fileno(stdin));
    if (saved_fd < 0)
    {
        return -1;
    }

    FILE *temp = tmpfile();
    if (!temp)
    {
        close(saved_fd);
        return -1;
    }

    fputs(script, temp);
    rewind(temp);

    if (dup2(fileno(temp), fileno(stdin)) != 0)
    {
        fclose(temp);
        close(saved_fd);
        return -1;
    }

    if (out_temp)
    {
        *out_temp = temp;
    }
    else
    {
        fclose(temp);
    }

    return saved_fd;
}

static int start_stdout_capture(FILE **capture_file, int *saved_fd)
{
    if (!capture_file || !saved_fd)
    {
        return -1;
    }

    fflush(stdout);

    *saved_fd = dup(fileno(stdout));
    if (*saved_fd < 0)
    {
        return -1;
    }

    *capture_file = tmpfile();
    if (!*capture_file)
    {
        close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    if (dup2(fileno(*capture_file), fileno(stdout)) != 0)
    {
        fclose(*capture_file);
        *capture_file = NULL;
        close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    return 0;
}

static char *stop_stdout_capture(FILE *capture_file, int saved_fd)
{
    if (capture_file)
    {
        fflush(stdout);
    }

    if (saved_fd >= 0)
    {
        dup2(saved_fd, fileno(stdout));
        close(saved_fd);
    }

    if (!capture_file)
    {
        return NULL;
    }

    long size = ftell(capture_file);
    if (size < 0)
    {
        fclose(capture_file);
        return NULL;
    }

    rewind(capture_file);
    char *buffer = malloc((size_t)size + 1);
    if (!buffer)
    {
        fclose(capture_file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, capture_file);
    buffer[read] = '\0';
    fclose(capture_file);
    return buffer;
}

static int count_csv_rows(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        return -1;
    }

    int lines = 0;
    int ch;
    int header_skipped = 0;

    while ((ch = fgetc(f)) != EOF)
    {
        if (ch == '\n')
        {
            if (!header_skipped)
            {
                header_skipped = 1;
            }
            else
            {
                ++lines;
            }
        }
    }

    fclose(f);
    return lines;
}

static void discard_and_free(char *buffer)
{
    if (buffer)
    {
        free(buffer);
    }
}

static void scenario_complete_workflow(void)
{
    const char *scenario = "Complete Workflow";
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/e2e-workflow.csv";
    remove(path);
    maintenance_set_csv_path(path);
    reset_storage();

    const char *record_inputs[] = {
        "Hydraulic Press\n\n2025-08-01\nChanged hydraulic fluid\ny\n",
        "CNC Mill\n\n2025-08-03\nRecalibrated spindle\ny\n",
        "Laser Cutter\n\n2025-08-05\nReplaced air filter\ny\n"};

    for (int i = 0; i < 3; ++i)
    {
        FILE *input_temp = NULL;
        int saved_fd = feed_input(record_inputs[i], &input_temp);
        SCENARIO_ASSERT_TRUE(scenario, "inject add_record input", saved_fd >= 0,
                              "prepared stdin for record %d", i + 1);

        FILE *output_capture = NULL;
        int saved_stdout = -1;
        start_stdout_capture(&output_capture, &saved_stdout);
        if (saved_fd >= 0)
        {
            add_record();
            restore_stdin(saved_fd, input_temp);
        }
        else
        {
            restore_stdin(saved_fd, input_temp);
        }
        discard_and_free(stop_stdout_capture(output_capture, saved_stdout));
    }

    SCENARIO_ASSERT_EQ_INT(scenario, "records after add", 3, record_count);

    int save_status = save_all_records();
    SCENARIO_ASSERT_TRUE(scenario, "save_all_records", save_status == 0,
                          "save_all_records returned %d", save_status);

    SCENARIO_ASSERT_EQ_INT(scenario, "csv row count", 3, count_csv_rows(path));

    reset_storage();
    int load_status = load_records();
    SCENARIO_ASSERT_TRUE(scenario, "load_records", load_status == 0,
                          "load_records returned %d", load_status);
    SCENARIO_ASSERT_EQ_INT(scenario, "records after reload", 3, record_count);
    SCENARIO_ASSERT_STREQ(scenario, "first record name", "Hydraulic Press", machineName[0]);

    remove(path);
    maintenance_set_csv_path(original);
    reset_storage();
}

static int contains_case_insensitive(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
    {
        return 0;
    }

    size_t n_len = strlen(needle);
    if (n_len == 0)
    {
        return 1;
    }

    for (const char *p = haystack; *p; ++p)
    {
        size_t matched = 0;
        while (needle[matched] && p[matched] &&
               tolower((unsigned char)p[matched]) == tolower((unsigned char)needle[matched]))
        {
            ++matched;
        }
        if (matched == n_len)
        {
            return 1;
        }
    }
    return 0;
}

static void scenario_data_persistence_and_search(void)
{
    const char *scenario = "Persistence & Search";
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/e2e-search.csv";
    FILE *f = fopen(path, "w");
    fprintf(f, "MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n");
    fprintf(f, "Precision Lathe,LT-10,2024-01-15,Align headstock\n");
    fprintf(f, "Hydraulic Press,HP-99,2024-02-20,Replace seals\n");
    fprintf(f, "Laser Cutter,LC-07,2024-03-05,Clean optics\n");
    fclose(f);

    maintenance_set_csv_path(path);
    reset_storage();

    int load_status = load_records();
    SCENARIO_ASSERT_TRUE(scenario, "load_records fixture", load_status == 0,
                          "load_records returned %d", load_status);
    SCENARIO_ASSERT_EQ_INT(scenario, "fixture record count", 3, record_count);

    int has_lathe = 0;
    int has_lc07 = 0;
    int found_unknown = 0;
    for (int i = 0; i < record_count; ++i)
    {
        if (contains_case_insensitive(machineName[i], "Lathe"))
        {
            has_lathe = 1;
        }
        if (strcmp(machineID[i], "LC-07") == 0)
        {
            has_lc07 = 1;
        }
        if (contains_case_insensitive(machineName[i], "Unknown") ||
            contains_case_insensitive(machineID[i], "Unknown"))
        {
            found_unknown = 1;
        }
    }

    SCENARIO_ASSERT_TRUE(scenario, "search by name", has_lathe, "Lathe fragment present in dataset");
    SCENARIO_ASSERT_TRUE(scenario, "search by id", has_lc07, "LC-07 ID present in dataset");
    SCENARIO_ASSERT_TRUE(scenario, "search not found", !found_unknown, "No record contains 'Unknown'");

    /* Execute search functions to exercise user journey, discarding output. */
    const char *search_inputs[] = {"Lathe\n", "LC-07\n", "Unknown\n"};
    for (int i = 0; i < 3; ++i)
    {
        FILE *input_temp = NULL;
        int saved_fd = feed_input(search_inputs[i], &input_temp);
        FILE *output_capture = NULL;
        int saved_stdout = -1;
        start_stdout_capture(&output_capture, &saved_stdout);
        search_records();
        discard_and_free(stop_stdout_capture(output_capture, saved_stdout));
        restore_stdin(saved_fd, input_temp);
    }

    remove(path);
    maintenance_set_csv_path(original);
    reset_storage();
}

static void scenario_update_and_delete(void)
{
    const char *scenario = "Update & Delete";
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/e2e-update.csv";
    remove(path);
    maintenance_set_csv_path(path);
    reset_storage();

    FILE *input_temp = NULL;
    int saved_fd = feed_input("Laser Cutter Pro\n\n2025-08-15\nReplace focusing lens\ny\n", &input_temp);
    FILE *output_capture = NULL;
    int saved_stdout = -1;
    start_stdout_capture(&output_capture, &saved_stdout);
    if (saved_fd >= 0)
    {
        add_record();
        restore_stdin(saved_fd, input_temp);
    }
    else
    {
        restore_stdin(saved_fd, input_temp);
    }
    discard_and_free(stop_stdout_capture(output_capture, saved_stdout));

    SCENARIO_ASSERT_EQ_INT(scenario, "record_count after add", 1, record_count);

    int save_status = save_all_records();
    SCENARIO_ASSERT_TRUE(scenario, "save after add", save_status == 0,
                          "save_all_records returned %d", save_status);

    input_temp = NULL;
    saved_fd = feed_input("1\nNew Laser Cutter\n2025-09-01\nRefined optics\ny\n", &input_temp);
    output_capture = NULL;
    saved_stdout = -1;
    start_stdout_capture(&output_capture, &saved_stdout);
    update_record();
    discard_and_free(stop_stdout_capture(output_capture, saved_stdout));
    restore_stdin(saved_fd, input_temp);

    SCENARIO_ASSERT_STREQ(scenario, "updated name", "New Laser Cutter", machineName[0]);
    SCENARIO_ASSERT_STREQ(scenario, "updated date", "2025-09-01", maintenanceDate[0]);

    save_status = save_all_records();
    SCENARIO_ASSERT_TRUE(scenario, "save after update", save_status == 0,
                          "save_all_records returned %d", save_status);

    input_temp = NULL;
    saved_fd = feed_input("1\ny\n", &input_temp);
    output_capture = NULL;
    saved_stdout = -1;
    start_stdout_capture(&output_capture, &saved_stdout);
    delete_record();
    discard_and_free(stop_stdout_capture(output_capture, saved_stdout));
    restore_stdin(saved_fd, input_temp);

    save_status = save_all_records();
    SCENARIO_ASSERT_TRUE(scenario, "save after delete", save_status == 0,
                          "save_all_records returned %d", save_status);

    reset_storage();
    int load_status = load_records();
    SCENARIO_ASSERT_TRUE(scenario, "reload after delete", load_status == 0,
                          "load_records returned %d", load_status);
    SCENARIO_ASSERT_EQ_INT(scenario, "record count after delete", 0, record_count);

    remove(path);
    maintenance_set_csv_path(original);
    reset_storage();
}

int main(void)
{
    scenario_complete_workflow();
    scenario_data_persistence_and_search();
    scenario_update_and_delete();

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

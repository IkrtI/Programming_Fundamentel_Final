#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "maintenance.h"
#include "maintenance_test_api.h"

extern char machineName[MAX_RECORDS][MAX_NAME];
extern char machineID[MAX_RECORDS][MAX_ID];
extern char maintenanceDate[MAX_RECORDS][MAX_DATE];
extern char maintenanceDetails[MAX_RECORDS][MAX_DETAILS];
extern int recordActive[MAX_RECORDS];

static int assertions_run = 0;
static int assertions_failed = 0;

#define ASSERT_TRUE(expr, message)                                                                 \
    do                                                                                             \
    {                                                                                              \
        ++assertions_run;                                                                          \
        if (!(expr))                                                                               \
        {                                                                                          \
            ++assertions_failed;                                                                   \
            fprintf(stderr, "[FAIL] %s:%d: %s -- %s\n", __func__, __LINE__, #expr, message);           \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            printf("      [PASS] %s\n", message);                                                     \
        }                                                                                          \
    } while (0)

#define ASSERT_EQ_INT(expected, actual, message)                                                    \
    do                                                                                             \
    {                                                                                              \
        ++assertions_run;                                                                          \
        int _expected = (expected);                                                                \
        int _actual = (actual);                                                                    \
        if (_expected != _actual)                                                                  \
        {                                                                                          \
            ++assertions_failed;                                                                   \
            fprintf(stderr, "[FAIL] %s:%d: expected %d but got %d -- %s\n", __func__, __LINE__,         \
                    _expected, _actual, message);                                                   \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            printf("      [PASS] %s (value=%d)\n", message, _expected);                               \
        }                                                                                          \
    } while (0)

#define ASSERT_STREQ(expected, actual, message)                                                     \
    do                                                                                             \
    {                                                                                              \
        ++assertions_run;                                                                          \
        const char *_expected = (expected);                                                        \
        const char *_actual = (actual);                                                            \
        int _match = 0;                                                                            \
        if (_expected == NULL && _actual == NULL)                                                  \
        {                                                                                          \
            _match = 1;                                                                            \
        }                                                                                          \
        else if (_expected != NULL && _actual != NULL && strcmp(_expected, _actual) == 0)          \
        {                                                                                          \
            _match = 1;                                                                            \
        }                                                                                          \
        if (!_match)                                                                               \
        {                                                                                          \
            ++assertions_failed;                                                                   \
            fprintf(stderr, "[FAIL] %s:%d: expected '%s' but got '%s' -- %s\n", __func__, __LINE__,    \
                    _expected ? _expected : "(null)", _actual ? _actual : "(null)", message);         \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            printf("      [PASS] %s\n", message);                                                     \
        }                                                                                          \
    } while (0)

static int redirect_stdin_from_string(const char *text, int *saved_fd, FILE **temp_file)
{
    if (!text || !saved_fd || !temp_file)
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

    if (fputs(text, *temp_file) == EOF)
    {
        fclose(*temp_file);
        *temp_file = NULL;
        close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    rewind(*temp_file);

    if (dup2(fileno(*temp_file), fileno(stdin)) < 0)
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

    if (dup2(fileno(*capture_file), fileno(stdout)) < 0)
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
    char *result = NULL;

    fflush(stdout);

    if (capture_file)
    {
        fflush(capture_file);
        long size = 0;
        if (fseek(capture_file, 0, SEEK_END) == 0)
        {
            long pos = ftell(capture_file);
            if (pos > 0)
            {
                size = pos;
            }
            fseek(capture_file, 0, SEEK_SET);
        }

        size_t alloc_size = (size_t)((size > 0) ? size : 0);
        result = (char *)malloc(alloc_size + 1);
        if (result)
        {
            size_t read = fread(result, 1, alloc_size, capture_file);
            result[read] = '\0';
        }
        else
        {
            result = (char *)malloc(1);
            if (result)
            {
                result[0] = '\0';
            }
        }

        fclose(capture_file);
    }

    if (saved_fd >= 0)
    {
        fflush(stdout);
        dup2(saved_fd, fileno(stdout));
        close(saved_fd);
    }

    if (!result)
    {
        result = (char *)malloc(1);
        if (result)
        {
            result[0] = '\0';
        }
    }

    return result;
}

static void reset_in_memory_records(void)
{
    record_count = 0;
    memset(machineName, 0, sizeof(machineName));
    memset(machineID, 0, sizeof(machineID));
    memset(maintenanceDate, 0, sizeof(maintenanceDate));
    memset(maintenanceDetails, 0, sizeof(maintenanceDetails));
    memset(recordActive, 0, sizeof(recordActive));
}

static int count_csv_data_lines(const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file)
    {
        return -1;
    }

    char buffer[1024];
    int count = 0;
    int is_header = 1;

    while (fgets(buffer, sizeof(buffer), file))
    {
        if (is_header)
        {
            is_header = 0;
            continue;
        }

        if (buffer[0] == '\0' || buffer[0] == '\n')
        {
            continue;
        }

        ++count;
    }

    fclose(file);
    return count;
}

static void remove_file_if_exists(const char *path)
{
    if (!path)
    {
        return;
    }
    remove(path);
}

static char *run_search_and_capture(const char *query)
{
    FILE *capture = NULL;
    int saved_stdout = -1;
    if (start_stdout_capture(&capture, &saved_stdout) != 0)
    {
        return NULL;
    }

    int saved_stdin = -1;
    FILE *temp_stdin = NULL;
    if (redirect_stdin_from_string(query, &saved_stdin, &temp_stdin) != 0)
    {
        char *discard = stop_stdout_capture(capture, saved_stdout);
        free(discard);
        return NULL;
    }

    search_records();

    restore_stdin(saved_stdin, temp_stdin);

    char *output = stop_stdout_capture(capture, saved_stdout);
    return output;
}

static void test_complete_maintenance_workflow(void)
{
    printf("\n=== END-TO-END TEST CASE 1: Complete Maintenance Workflow ===\n");
    printf("📋 Scenario: Add → Save → Reload → Display\n\n");

    const char *test_path = "tests/e2e-workflow.csv";
    char original_path[CSV_PATH_MAX];
    snprintf(original_path, sizeof(original_path), "%s", maintenance_get_csv_path());

    printf("🔹 ARRANGE\n");
    printf("   - Configure test file: %s\n", test_path);
    remove_file_if_exists(test_path);
    ASSERT_EQ_INT(0, maintenance_set_csv_path(test_path), "Configured CSV path for test run");
    reset_in_memory_records();

    struct
    {
        const char *name;
        const char *id;
        const char *date;
        const char *details;
    } records[] = {
        {"Hydraulic Press", "HP-001", "2025-08-01", "Changed hydraulic fluid"},
        {"CNC Mill", "CNC-12", "2025-08-03", "Recalibrated spindle"},
        {"Laser Cutter", "LC-07", "2025-08-05", "Replaced air filter"},
    };

    size_t record_total = sizeof(records) / sizeof(records[0]);
    printf("   - Records to add: %zu\n", record_total);

    printf("\n🔹 ACT\n");
    printf("   Step 1: Add new entries via add_record()\n");
    for (size_t i = 0; i < record_total; ++i)
    {
        char input[512];
        snprintf(input, sizeof(input), "%s\n%s\n%s\n%s\n",
                 records[i].name, records[i].id, records[i].date, records[i].details);

        int saved_fd = -1;
        FILE *temp = NULL;
        ASSERT_EQ_INT(0, redirect_stdin_from_string(input, &saved_fd, &temp), "Simulate user input for new record");
        add_record();
        restore_stdin(saved_fd, temp);
    }
    ASSERT_EQ_INT((int)record_total, record_count, "Record count matches entries added");

    printf("   Step 2: Persist records to CSV\n");
    ASSERT_EQ_INT(0, save_all_records(), "Save all records to disk");
    ASSERT_TRUE(access(test_path, F_OK) == 0, "CSV file created successfully");

    printf("   Step 3: Exercise reload_records_with_warning()\n");
    ASSERT_EQ_INT(0, reload_records_with_warning(), "Reload records without warnings");
    ASSERT_EQ_INT((int)record_total, record_count, "Record count remains correct after reload");

    printf("   Step 4: Clear memory and load from disk again\n");
    reset_in_memory_records();
    ASSERT_EQ_INT(0, load_records(), "Load records back from CSV");

    printf("\n🔹 ASSERT\n");
    ASSERT_EQ_INT((int)record_total, record_count, "Record count after reload from file");
    for (size_t i = 0; i < record_total; ++i)
    {
        ASSERT_STREQ(records[i].name, machineName[i], "Machine name matches original data");
        ASSERT_STREQ(records[i].id, machineID[i], "Machine ID matches original data");
        ASSERT_STREQ(records[i].date, maintenanceDate[i], "Maintenance date matches original data");
        ASSERT_STREQ(records[i].details, maintenanceDetails[i], "Maintenance details match original data");
    }

    int csv_lines = count_csv_data_lines(test_path);
    ASSERT_EQ_INT((int)record_total, csv_lines, "CSV data row count matches");

    FILE *capture = NULL;
    int saved_stdout = -1;
    ASSERT_EQ_INT(0, start_stdout_capture(&capture, &saved_stdout), "Begin capturing display_records() output");
    display_records();
    char *display_output = stop_stdout_capture(capture, saved_stdout);
    ASSERT_TRUE(display_output != NULL, "Captured display output successfully");
    for (size_t i = 0; i < record_total; ++i)
    {
        ASSERT_TRUE(strstr(display_output, records[i].id) != NULL, "Record appears in rendered table");
    }
    free(display_output);

    remove_file_if_exists(test_path);
    reset_in_memory_records();
    maintenance_set_csv_path(original_path);

    printf("\n✅ END-TO-END TEST CASE 1: COMPLETED\n");
}

static void test_data_persistence_and_search(void)
{
    printf("\n=== END-TO-END TEST CASE 2: Data Persistence and Search ===\n");
    printf("📋 Scenario: Load → Search → Verify results\n\n");

    const char *test_path = "tests/e2e-search.csv";
    char original_path[CSV_PATH_MAX];
    snprintf(original_path, sizeof(original_path), "%s", maintenance_get_csv_path());

    printf("🔹 ARRANGE\n");
    printf("   - Create CSV fixture for search tests\n");
    remove_file_if_exists(test_path);

    FILE *file = fopen(test_path, "w");
    ASSERT_TRUE(file != NULL, "Created CSV fixture for search validation");
    if (file)
    {
        fputs("MachineName,MachineID,MaintenanceDate,MaintenanceDetails,Active\n", file);
        fputs("Hydraulic Press,HP-001,2025-08-01,Changed hydraulic fluid,1\n", file);
        fputs("CNC Mill,CNC-12,2025-08-03,Recalibrated spindle,1\n", file);
        fputs("3D Printer,PR-22,2025-08-04,Updated firmware,1\n", file);
        fclose(file);
    }

    ASSERT_EQ_INT(0, maintenance_set_csv_path(test_path), "Set CSV path for search validation");
    reset_in_memory_records();

    printf("\n🔹 ACT\n");
    ASSERT_EQ_INT(0, load_records(), "Load records from fixture file");

    printf("   Step: Execute multiple search queries\n");
    char *result_by_name = run_search_and_capture("CNC\n");
    ASSERT_TRUE(result_by_name != NULL, "Captured search output by name");

    char *result_by_id = run_search_and_capture("PR-22\n");
    ASSERT_TRUE(result_by_id != NULL, "Captured search output by ID");

    char *result_not_found = run_search_and_capture("UNKNOWN\n");
    ASSERT_TRUE(result_not_found != NULL, "Captured output when no records match");

    printf("\n🔹 ASSERT\n");
    ASSERT_EQ_INT(3, record_count, "Record count loaded from CSV");
    ASSERT_STREQ("Hydraulic Press", machineName[0], "Row 1 matches CSV contents");
    ASSERT_STREQ("CNC Mill", machineName[1], "Row 2 matches CSV contents");
    ASSERT_STREQ("3D Printer", machineName[2], "Row 3 matches CSV contents");

    ASSERT_TRUE(strstr(result_by_name, "CNC Mill") != NULL, "Name search output contains expected record");
    ASSERT_TRUE(strstr(result_by_name, "Total 1 record") != NULL, "Name search summary shows correct total");

    ASSERT_TRUE(strstr(result_by_id, "PR-22") != NULL, "ID search output contains expected record");
    ASSERT_TRUE(strstr(result_by_id, "Total 1 record") != NULL, "ID search summary shows correct total");

    ASSERT_TRUE(strstr(result_not_found, "No records found") != NULL, "No-match search reports informative message");

    int csv_lines = count_csv_data_lines(test_path);
    ASSERT_EQ_INT(3, csv_lines, "CSV data row count for search fixture");

    free(result_by_name);
    free(result_by_id);
    free(result_not_found);

    remove_file_if_exists(test_path);
    reset_in_memory_records();
    maintenance_set_csv_path(original_path);

    printf("\n✅ END-TO-END TEST CASE 2: COMPLETED\n");
}

static void test_update_and_delete_workflow(void)
{
    printf("\n=== END-TO-END TEST CASE 3: Update and Delete Workflow ===\n");
    printf("📋 Scenario: Add → Save → Update → Archive → Recover → Delete\n\n");

    const char *test_path = "tests/e2e-update-delete.csv";
    char original_path[CSV_PATH_MAX];
    snprintf(original_path, sizeof(original_path), "%s", maintenance_get_csv_path());

    printf("🔹 ARRANGE\n");
    remove_file_if_exists(test_path);
    ASSERT_EQ_INT(0, maintenance_set_csv_path(test_path), "Set CSV path for update/delete test");
    reset_in_memory_records();

    printf("\n🔹 ACT\n");
    printf("   Step 1: Insert initial record\n");
    {
        char input[] = "Laser Cutter\nLC-07\n2025-08-05\nInitial inspection\n";
        int saved_fd = -1;
        FILE *temp = NULL;
        ASSERT_EQ_INT(0, redirect_stdin_from_string(input, &saved_fd, &temp), "Simulate adding initial record");
        add_record();
        restore_stdin(saved_fd, temp);
    }
    ASSERT_EQ_INT(1, record_count, "In-memory record count is 1 after insert");
    ASSERT_EQ_INT(0, save_all_records(), "Saved file after initial insert");

    printf("   Step 2: Reload before updating\n");
    reset_in_memory_records();
    ASSERT_EQ_INT(0, load_records(), "Load records back in before update");

    printf("   Step 3: Update record via update_record()\n");
    {
        char input[] = "LC-07\nLaser Cutter Pro\n2025-08-15\nReplaced focusing lens\n";
        int saved_fd = -1;
        FILE *temp = NULL;
        ASSERT_EQ_INT(0, redirect_stdin_from_string(input, &saved_fd, &temp), "Simulate update input");
        update_record();
        restore_stdin(saved_fd, temp);
    }

    ASSERT_STREQ("Laser Cutter Pro", machineName[0], "Machine name updated");
    ASSERT_STREQ("2025-08-15", maintenanceDate[0], "Maintenance date updated");
    ASSERT_STREQ("Replaced focusing lens", maintenanceDetails[0], "Maintenance details updated");

    ASSERT_EQ_INT(0, save_all_records(), "Saved file after update");

    printf("   Step 4: Reload to confirm updated data\n");
    reset_in_memory_records();
    ASSERT_EQ_INT(0, load_records(), "Load records after update");
    ASSERT_STREQ("Laser Cutter Pro", machineName[0], "Reloaded data reflects new values");

    printf("   Step 5: Archive record via delete_record()\n");
    {
        char input[] = "LC-07\n";
        int saved_fd = -1;
        FILE *temp = NULL;
        ASSERT_EQ_INT(0, redirect_stdin_from_string(input, &saved_fd, &temp), "Simulate delete input");
        delete_record();
        restore_stdin(saved_fd, temp);
    }

    ASSERT_EQ_INT(1, record_count, "Record count remains 1 after soft delete");
    ASSERT_EQ_INT(0, recordActive[0], "Record is marked as deleted after soft delete");
    ASSERT_EQ_INT(0, save_all_records(), "Saved file after soft delete");

    {
        FILE *capture = NULL;
        int saved_stdout = -1;
        ASSERT_EQ_INT(0, start_stdout_capture(&capture, &saved_stdout),
                      "Begin capturing display_records() after soft delete");
        display_records();
        char *display_output = stop_stdout_capture(capture, saved_stdout);
        ASSERT_TRUE(display_output != NULL, "Captured display output after soft delete");
        ASSERT_TRUE(strstr(display_output, "LC-07") == NULL,
                    "Archived record is hidden from list view");
        ASSERT_TRUE(strstr(display_output, "No active maintenance records") != NULL,
                    "Display indicates there are no active records");
        free(display_output);
    }

    printf("   Step 6: Reload to confirm archived status\n");
    reset_in_memory_records();
    ASSERT_EQ_INT(0, load_records(), "Load file after soft deletion");
    ASSERT_EQ_INT(1, record_count, "Record count persists after reload");
    ASSERT_EQ_INT(0, recordActive[0], "Reloaded record remains archived");

    printf("   Step 7: Recover record via manage_deleted_records()\n");
    {
        char input[] = "1\nLC-07\n\n";
        int saved_fd = -1;
        FILE *temp = NULL;
        ASSERT_EQ_INT(0, redirect_stdin_from_string(input, &saved_fd, &temp), "Simulate recovery input");
        manage_deleted_records();
        restore_stdin(saved_fd, temp);
    }

    ASSERT_EQ_INT(1, recordActive[0], "Record is active after recovery");
    ASSERT_EQ_INT(0, save_all_records(), "Saved file after recovery");

    printf("   Step 8: Reload to confirm recovered status\n");
    reset_in_memory_records();
    ASSERT_EQ_INT(0, load_records(), "Load file after recovery");
    ASSERT_EQ_INT(1, record_count, "Record count persists after recovery reload");
    ASSERT_EQ_INT(1, recordActive[0], "Reloaded record remains active");

    printf("   Step 9: Archive record again via delete_record()\n");
    {
        char input[] = "LC-07\n";
        int saved_fd = -1;
        FILE *temp = NULL;
        ASSERT_EQ_INT(0, redirect_stdin_from_string(input, &saved_fd, &temp), "Simulate second archive input");
        delete_record();
        restore_stdin(saved_fd, temp);
    }

    ASSERT_EQ_INT(1, record_count, "Record count remains 1 after second soft delete");
    ASSERT_EQ_INT(0, recordActive[0], "Record is marked as deleted before permanent removal");
    ASSERT_EQ_INT(0, save_all_records(), "Saved file after second soft delete");

    printf("   Step 10: Permanently delete record via manage_deleted_records()\n");
    {
        char input[] = "2\nLC-07\n\n";
        int saved_fd = -1;
        FILE *temp = NULL;
        ASSERT_EQ_INT(0, redirect_stdin_from_string(input, &saved_fd, &temp), "Simulate permanent delete input");
        manage_deleted_records();
        restore_stdin(saved_fd, temp);
    }

    ASSERT_EQ_INT(0, record_count, "Record count is zero after permanent delete");
    ASSERT_EQ_INT(0, save_all_records(), "Saved file after permanent delete");

    printf("   Step 11: Reload to confirm data removal\n");
    reset_in_memory_records();
    ASSERT_EQ_INT(0, load_records(), "Load file after permanent deletion");
    ASSERT_EQ_INT(0, record_count, "Record count remains zero after reload");

    int csv_lines = count_csv_data_lines(test_path);
    ASSERT_EQ_INT(0, csv_lines, "CSV file has no data rows after permanent deletion");

    remove_file_if_exists(test_path);
    reset_in_memory_records();
    maintenance_set_csv_path(original_path);

    printf("\n✅ END-TO-END TEST CASE 3: COMPLETED\n");
}

int main(void)
{
    printf("\n===============================================\n");
    printf("🚀 Starting End-to-End Machine Maintenance Suite\n");
    printf("===============================================\n");

    test_complete_maintenance_workflow();
    test_data_persistence_and_search();
    test_update_and_delete_workflow();

    printf("\n===============================================\n");
    printf("End-to-End Test Summary\n");
    printf("  - Assertions executed: %d\n", assertions_run);
    printf("  - Assertions failed: %d\n", assertions_failed);
    printf("===============================================\n\n");

    if (assertions_failed == 0)
    {
        printf("🎉 Result: ALL END-TO-END TESTS PASSED\n");
        return 0;
    }

    printf("⚠️ Result: End-to-End tests encountered failures\n");
    return 1;
}

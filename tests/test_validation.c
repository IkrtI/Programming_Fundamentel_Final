#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "maintenance.h"

static int tests_run = 0;
static int tests_failed = 0;

#define EXPECT_TRUE(expr)                                                                    \
    do                                                                                       \
    {                                                                                        \
        ++tests_run;                                                                         \
        if (!(expr))                                                                         \
        {                                                                                    \
            ++tests_failed;                                                                  \
            fprintf(stderr, "EXPECT_TRUE failed: %s (line %d)\n", #expr, __LINE__);          \
        }                                                                                    \
    } while (0)

#define EXPECT_STREQ(expected, actual)                                                       \
    do                                                                                       \
    {                                                                                        \
        ++tests_run;                                                                         \
        if (strcmp((expected), (actual)) != 0)                                               \
        {                                                                                    \
            ++tests_failed;                                                                  \
            fprintf(stderr,                                                                  \
                    "EXPECT_STREQ failed: expected '%s' but got '%s' (line %d)\n",           \
                    (expected), (actual), __LINE__);                                         \
        }                                                                                    \
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

static void test_trim_whitespace(void)
{
    char str1[] = "  Hello World  ";
    trim_whitespace(str1);
    EXPECT_STREQ("Hello World", str1);

    char str2[] = "   \t  ";
    trim_whitespace(str2);
    EXPECT_STREQ("", str2);

    char str3[] = "\tSpacing\n";
    trim_whitespace(str3);
    EXPECT_STREQ("Spacing", str3);
}

static void test_sanitize_input(void)
{
    char str1[] = "  Value with spaces \r\n";
    sanitize_input(str1);
    EXPECT_STREQ("Value with spaces", str1);

    char str2[] = "\tAnother Value\n";
    sanitize_input(str2);
    EXPECT_STREQ("Another Value", str2);
}

static void test_safe_input_eof_without_newline(void)
{
    int saved_fd = -1;
    FILE *temp = NULL;
    EXPECT_TRUE(redirect_stdin_from_string("EOF without newline", &saved_fd, &temp) == 0);

    char buffer[64];
    int status = safe_input(buffer, sizeof(buffer), "Prompt: ");

    restore_stdin(saved_fd, temp);

    EXPECT_TRUE(status == INPUT_OK);
    EXPECT_STREQ("EOF without newline", buffer);
}

static void test_safe_input_overflow_flushes(void)
{
    int saved_fd = -1;
    FILE *temp = NULL;
    const char *input = "This line is definitely longer than the buffer size provided\nNext line\n";
    EXPECT_TRUE(redirect_stdin_from_string(input, &saved_fd, &temp) == 0);

    char buffer[32];
    int status = safe_input(buffer, sizeof(buffer), "Prompt: ");
    EXPECT_TRUE(status == INPUT_TOO_LONG);
    EXPECT_STREQ("", buffer);

    status = safe_input(buffer, sizeof(buffer), "Prompt: ");
    EXPECT_TRUE(status == INPUT_OK);
    EXPECT_STREQ("Next line", buffer);

    restore_stdin(saved_fd, temp);
}

static void test_is_non_empty(void)
{
    EXPECT_TRUE(is_non_empty("Hello"));
    EXPECT_TRUE(!is_non_empty(""));
    EXPECT_TRUE(!is_non_empty(NULL));
}

static void test_contains_disallowed_csv_chars(void)
{
    EXPECT_TRUE(contains_disallowed_csv_chars("bad,comma"));
    EXPECT_TRUE(contains_disallowed_csv_chars("bad\"quote"));
    EXPECT_TRUE(!contains_disallowed_csv_chars("good value"));
}

static void test_is_valid_machine_name(void)
{
    EXPECT_TRUE(is_valid_machine_name("Lathe 100"));
    EXPECT_TRUE(!is_valid_machine_name(""));
    EXPECT_TRUE(!is_valid_machine_name("Bad,Name"));

    char non_printable[] = "Valid\x01";
    EXPECT_TRUE(!is_valid_machine_name(non_printable));
}

static void test_is_valid_machine_id(void)
{
    EXPECT_TRUE(is_valid_machine_id("123"));
    EXPECT_TRUE(is_valid_machine_id("ID-123_45.6"));
    EXPECT_TRUE(is_valid_machine_id("A.B_C-1"));
    EXPECT_TRUE(!is_valid_machine_id(""));
    EXPECT_TRUE(!is_valid_machine_id("with space"));
    EXPECT_TRUE(!is_valid_machine_id("comma,here"));
    EXPECT_TRUE(!is_valid_machine_id("invalid#char"));
    EXPECT_TRUE(!is_valid_machine_id("id@123"));
    EXPECT_TRUE(!is_valid_machine_id(NULL));
}

static void test_is_valid_date(void)
{
    EXPECT_TRUE(is_valid_date("2024-02-29"));
    EXPECT_TRUE(!is_valid_date("2024-02-30"));
    EXPECT_TRUE(!is_valid_date("2024-13-01"));
    EXPECT_TRUE(!is_valid_date("2024/02/01"));
    EXPECT_TRUE(!is_valid_date("20240201"));
}

static void test_is_valid_details(void)
{
    EXPECT_TRUE(is_valid_details("Replaced filter"));
    EXPECT_TRUE(!is_valid_details(""));
    EXPECT_TRUE(!is_valid_details("Contains,comma"));

    char invalid[] = "Bad\nDetail";
    EXPECT_TRUE(!is_valid_details(invalid));
}

static void test_contains_cancel_signal(void)
{
    char with_cancel[] = {'A', 0x18, 'B', '\0'};
    char no_cancel[] = "Normal text";

    EXPECT_TRUE(contains_cancel_signal(with_cancel));
    EXPECT_TRUE(!contains_cancel_signal(no_cancel));
    EXPECT_TRUE(!contains_cancel_signal(NULL));
}

static void test_is_record_storage_full(void)
{
    int original = record_count;

    record_count = 0;
    EXPECT_TRUE(!is_record_storage_full());

    record_count = MAX_RECORDS;
    EXPECT_TRUE(is_record_storage_full());

    record_count = -1;
    EXPECT_TRUE(is_record_storage_full());

    record_count = original;
}

static void test_csv_path_management(void)
{
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    EXPECT_TRUE(maintenance_set_csv_path("tests/tmp-maintenance.csv") == 0);
    EXPECT_STREQ("tests/tmp-maintenance.csv", maintenance_get_csv_path());

    EXPECT_TRUE(maintenance_set_csv_path(NULL) == -1);
    EXPECT_STREQ("tests/tmp-maintenance.csv", maintenance_get_csv_path());

    EXPECT_TRUE(maintenance_set_csv_path("") == -1);
    EXPECT_STREQ("tests/tmp-maintenance.csv", maintenance_get_csv_path());

    char long_path[CSV_PATH_MAX + 5];
    memset(long_path, 'a', sizeof(long_path));
    long_path[sizeof(long_path) - 1] = '\0';
    EXPECT_TRUE(maintenance_set_csv_path(long_path) == -1);
    EXPECT_STREQ("tests/tmp-maintenance.csv", maintenance_get_csv_path());

    EXPECT_TRUE(maintenance_set_csv_path(original) == 0);
    EXPECT_STREQ(original, maintenance_get_csv_path());
}

static void test_reload_records_with_warning(void)
{
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    EXPECT_TRUE(maintenance_set_csv_path("tests/nonexistent.csv") == 0);

    FILE *capture = tmpfile();
    EXPECT_TRUE(capture != NULL);

    int saved_stdout_fd = dup(fileno(stdout));
    EXPECT_TRUE(saved_stdout_fd >= 0);

    int capture_fd = fileno(capture);
    EXPECT_TRUE(capture_fd >= 0);

    fflush(stdout);
    EXPECT_TRUE(dup2(capture_fd, fileno(stdout)) >= 0);

    int rc = reload_records_with_warning();

    fflush(stdout);
    EXPECT_TRUE(dup2(saved_stdout_fd, fileno(stdout)) >= 0);
    close(saved_stdout_fd);

    rewind(capture);
    char buffer[256];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, capture);
    buffer[read] = '\0';
    fclose(capture);

    EXPECT_TRUE(rc != 0);
    EXPECT_TRUE(strstr(buffer, "Warning: Failed to reload records") != NULL);

    EXPECT_TRUE(maintenance_set_csv_path(original) == 0);
    EXPECT_STREQ(original, maintenance_get_csv_path());
}

static void test_ensure_csv_exists_creates_blank_when_declined(void)
{
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/tmp-blank.csv";
    unlink(path);
    EXPECT_TRUE(maintenance_set_csv_path(path) == 0);

    int saved_fd = -1;
    FILE *temp = NULL;
    EXPECT_TRUE(redirect_stdin_from_string("n\n", &saved_fd, &temp) == 0);

    EXPECT_TRUE(ensure_csv_exists() == 0);

    restore_stdin(saved_fd, temp);

    FILE *created = fopen(path, "r");
    EXPECT_TRUE(created != NULL);
    if (created)
    {
        char buffer[256];
        size_t read = fread(buffer, 1, sizeof(buffer) - 1, created);
        buffer[read] = '\0';
        fclose(created);

        EXPECT_STREQ("MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n", buffer);
    }

    unlink(path);
    EXPECT_TRUE(maintenance_set_csv_path(original) == 0);
}

static void test_ensure_csv_exists_copies_sample_when_requested(void)
{
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/tmp-copy.csv";
    unlink(path);
    EXPECT_TRUE(maintenance_set_csv_path(path) == 0);

    int saved_fd = -1;
    FILE *temp = NULL;
    EXPECT_TRUE(redirect_stdin_from_string("y\n", &saved_fd, &temp) == 0);

    EXPECT_TRUE(ensure_csv_exists() == 0);

    restore_stdin(saved_fd, temp);

    FILE *created = fopen(path, "r");
    EXPECT_TRUE(created != NULL);

    FILE *example = fopen("maintenance-example.csv", "r");
    EXPECT_TRUE(example != NULL);

    if (created && example)
    {
        char created_buffer[8192];
        char example_buffer[8192];

        size_t created_read = fread(created_buffer, 1, sizeof(created_buffer), created);
        size_t example_read = fread(example_buffer, 1, sizeof(example_buffer), example);

        fclose(created);
        fclose(example);

        EXPECT_TRUE(created_read == example_read);
        if (created_read == example_read)
        {
            EXPECT_TRUE(memcmp(created_buffer, example_buffer, created_read) == 0);
        }
    }
    else
    {
        if (created)
        {
            fclose(created);
        }
        if (example)
        {
            fclose(example);
        }
    }

    unlink(path);
    EXPECT_TRUE(maintenance_set_csv_path(original) == 0);
}

static void test_ensure_csv_exists_preserves_existing_file(void)
{
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/tmp-existing.csv";
    unlink(path);

    FILE *existing = fopen(path, "w");
    EXPECT_TRUE(existing != NULL);
    if (existing)
    {
        fputs("existing data\n", existing);
        fclose(existing);
    }

    EXPECT_TRUE(maintenance_set_csv_path(path) == 0);
    EXPECT_TRUE(ensure_csv_exists() == 0);

    FILE *check = fopen(path, "r");
    EXPECT_TRUE(check != NULL);
    if (check)
    {
        char buffer[64];
        size_t read = fread(buffer, 1, sizeof(buffer) - 1, check);
        buffer[read] = '\0';
        fclose(check);
        EXPECT_STREQ("existing data\n", buffer);
    }

    unlink(path);
    EXPECT_TRUE(maintenance_set_csv_path(original) == 0);
}

int main(void)
{
    test_trim_whitespace();
    test_sanitize_input();
    test_safe_input_eof_without_newline();
    test_safe_input_overflow_flushes();
    test_is_non_empty();
    test_contains_disallowed_csv_chars();
    test_is_valid_machine_name();
    test_is_valid_machine_id();
    test_is_valid_date();
    test_is_valid_details();
    test_contains_cancel_signal();
    test_is_record_storage_full();
    test_csv_path_management();
    test_reload_records_with_warning();
    test_ensure_csv_exists_creates_blank_when_declined();
    test_ensure_csv_exists_copies_sample_when_requested();
    test_ensure_csv_exists_preserves_existing_file();

    if (tests_failed != 0)
    {
        fprintf(stderr, "\n%d of %d expectations failed.\n", tests_failed, tests_run);
        return 1;
    }

    printf("All %d expectations passed.\n", tests_run);
    return 0;
}

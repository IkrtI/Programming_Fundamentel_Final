#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#if defined(ENABLE_INTERNAL_TESTS)
#include <stdarg.h>
#if defined(_WIN32)
#include <io.h>
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif

#if !defined(_WIN32)
#include <unistd.h>
#endif
#if !defined(_WIN32) && !defined(UNIT_TEST)
#include <termios.h>
#endif

#include "maintenance.h"

static char csv_file_path[CSV_PATH_MAX] = CSV_FILE_DEFAULT;
static const char *SAMPLE_CSV_FILE = "maintenance-example.csv";
static const char CSV_HEADER[] = "MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n";

/* Portable UNUSED function attribute to silence unused warnings on some builds */
#if !defined(UNUSED_FN)
# if defined(__GNUC__) || defined(__clang__)
#  define UNUSED_FN __attribute__((unused))
# else
#  define UNUSED_FN
# endif
#endif

char machineName[MAX_RECORDS][MAX_NAME];
char machineID[MAX_RECORDS][MAX_ID];
char maintenanceDate[MAX_RECORDS][MAX_DATE];
char maintenanceDetails[MAX_RECORDS][MAX_DETAILS];
int record_count = 0;

static record_result_t add_record_direct(const char *name, const char *id,
                                         const char *date, const char *details);
static record_result_t delete_record_direct(const char *id);
#ifndef UNIT_TEST
static int run_unit_test_suite(void);
static int run_end_to_end_suite(void);
#if defined(ENABLE_INTERNAL_TESTS)
static int run_internal_unit_tests(void);
static int run_internal_e2e_tests(void);
#endif
#endif

static volatile sig_atomic_t interrupt_requested = 0;
static volatile sig_atomic_t exit_requested = 0;

static void print_cancel_message(void)
{
    printf("\nOperation cancelled. Returning to Machine Maintenance Manager.\n");
}
static void UNUSED_FN clear_record_storage(void);
static void UNUSED_FN handle_sigint(int signal);
static int UNUSED_FN exit_program(int status);
static int UNUSED_FN show_csv_control_menu(void);
static int read_csv_menu_choice(size_t csv_count, int *selected_index, char *command);
static int prompt_csv_index_selection(size_t csv_count, const char *prompt, int *out_index);
static int has_csv_extension(const char *name);
static int is_regular_file(const char *path, const struct dirent *entry);
static int compare_csv_names(const void *lhs, const void *rhs);
static int compare_records_by_schedule(const void *lhs, const void *rhs);
static void render_csv_menu(char *const *csv_files, size_t csv_count);
static void UNUSED_FN handle_addon_menu(void);
static void display_addon_menu(void);
static int read_addon_choice(void);
static void resequence_machine_ids(void);
static void normalize_imported_details(char *details);
static void print_record_table_header(void);
static void print_record_table_footer(void);
static void print_record_table_row(int index);
static void print_record_preview_from_values(const char *name, const char *id,
                                             const char *date, const char *details);
static int get_terminal_height(void);
static int calculate_records_per_page(void);
static void clear_console_output(void);
static void UNUSED_FN clear_function_log(void);
static void flush_line(void);
static void request_exit(void);
static int prompt_copy_from_sample(void);
static int write_blank_csv(const char *path);
static int copy_example_csv(const char *path);
static int copy_csv_file(const char *source, const char *destination);
static int interpret_paging_arrow(const char *input);
static int read_paging_command(char *buffer, size_t size);
static int contains_back_signal(const char *str);
static int parse_machine_id_value(const char *id, int *out_value);
static int normalize_machine_id_value(int value, char *buffer, size_t size);
static int is_machine_id_unique(const char *id);
static int find_smallest_available_machine_id(void);
static int prompt_machine_id(char *buffer, size_t size);
static int string_contains_case_insensitive(const char *haystack, const char *needle);
static int prompt_yes_no_common(const char *prompt, const char *invalid_message,
                                int allow_blank_default, int default_choice,
                                int error_default, int *out_choice);
static int prompt_confirmation(const char *prompt, int *out_confirmed);

#if !defined(_WIN32) && !defined(UNIT_TEST)
static void restore_terminal_settings(void);
static int configure_terminal_shortcuts(void);
static struct termios original_termios;
static int terminal_configured = 0;
#endif

/* Forward declarations for public functions implemented in this file */
int safe_input(char *buffer, int size, const char *prompt);
int prompt_with_validation(char *buffer, int size, const char *prompt,
                           int (*validator)(const char *), const char *error_message);
void trim_whitespace(char *str);
void sanitize_input(char *buffer);
int is_non_empty(const char *str);
int contains_disallowed_csv_chars(const char *str);
int contains_cancel_signal(const char *str);
int is_valid_machine_name(const char *str);
int is_valid_machine_id(const char *str);
int is_valid_date(const char *str);
int is_valid_details(const char *str);
int prompt_optional_update(const char *prompt, char *dest, int dest_size,
                           int (*validator)(const char *), const char *error_message);
int maintenance_set_csv_path(const char *path);
const char *maintenance_get_csv_path(void);
int ensure_csv_exists(void);
int load_records(void);
int save_all_records(void);
int reload_records_with_warning(void);
int is_record_storage_full(void);
void display_records(void);
void add_record(void);
void search_records(void);
void update_record(void);
void delete_record(void);

static int handle_prompt_result(int status, const char *error_message)
{
    if (status == INPUT_CANCELLED)
    {
        print_cancel_message();
        return 1;
    }

    if (status == INPUT_EXIT)
    {
        request_exit();
        return 1;
    }

    if (status == INPUT_BACK)
    {
        print_cancel_message();
        return 1;
    }

    if (status == INPUT_ERROR)
    {
        if (error_message)
        {
            printf("%s\n", error_message);
        }
        return 1;
    }

    return 0;
}

static void request_exit(void)
{
    exit_requested = 1;
    interrupt_requested = 1;
}

#ifndef UNIT_TEST
static int UNUSED_FN exit_program(int status)
{
#if !defined(_WIN32)
    restore_terminal_settings();
#endif
    clear_record_storage();
    return status;
}
#endif

#if !defined(_WIN32) && !defined(UNIT_TEST)
static void restore_terminal_settings(void)
{
    if (!terminal_configured)
    {
        return;
    }

    if (tcsetattr(STDIN_FILENO, TCSANOW, &original_termios) == 0)
    {
        terminal_configured = 0;
    }
}

static int configure_terminal_shortcuts(void)
{
    if (!isatty(STDIN_FILENO))
    {
        return 0;
    }

    if (terminal_configured)
    {
        return 0;
    }

    if (tcgetattr(STDIN_FILENO, &original_termios) != 0)
    {
        return -1;
    }

    struct termios modified = original_termios;
#ifdef VEOL
    modified.c_cc[VEOL] = 0x18; /* Ctrl+X behaves like Enter */
#endif
#ifdef VEOL2
    modified.c_cc[VEOL2] = 0x1A; /* Ctrl+Z behaves like Enter */
#endif
#ifdef VSUSP
#ifdef _POSIX_VDISABLE
    modified.c_cc[VSUSP] = _POSIX_VDISABLE;
#else
    modified.c_cc[VSUSP] = 0;
#endif
#endif

    if (tcsetattr(STDIN_FILENO, TCSANOW, &modified) != 0)
    {
        return -1;
    }

    terminal_configured = 1;
    atexit(restore_terminal_settings);
    return 0;
}
#endif

static int has_csv_extension(const char *name)
{
    if (!name)
    {
        return 0;
    }

    const char *dot = strrchr(name, '.');
    if (!dot || dot == name)
    {
        return 0;
    }

    const char *ext = dot + 1;
    if (tolower((unsigned char)ext[0]) == 'c' &&
        tolower((unsigned char)ext[1]) == 's' &&
        tolower((unsigned char)ext[2]) == 'v' &&
        ext[3] == '\0')
    {
        return 1;
    }

    return 0;
}

static int is_regular_file(const char *path, const struct dirent *entry)
{
    if (!path || !entry)
    {
        return 0;
    }

#ifdef DT_REG
    if (entry->d_type == DT_REG)
    {
        return 1;
    }

    if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_LNK)
    {
        return 0;
    }
#endif

    struct stat st;
    if (stat(path, &st) != 0)
    {
        return 0;
    }

    return S_ISREG(st.st_mode);
}

static int compare_csv_names(const void *lhs, const void *rhs)
{
    const char *const *a = lhs;
    const char *const *b = rhs;
    if (!a || !b || !*a || !*b)
    {
        return 0;
    }

    return strcmp(*a, *b);
}

static int compare_records_by_schedule(const void *lhs, const void *rhs)
{
    if (!lhs || !rhs)
    {
        return 0;
    }

    int left_index = *(const int *)lhs;
    int right_index = *(const int *)rhs;

    if (left_index == right_index)
    {
        return 0;
    }

    const char *left_date = maintenanceDate[left_index];
    const char *right_date = maintenanceDate[right_index];

    int left_empty = (!left_date || left_date[0] == '\0');
    int right_empty = (!right_date || right_date[0] == '\0');

    if (left_empty && right_empty)
    {
        int name_cmp = strcmp(machineName[left_index], machineName[right_index]);
        if (name_cmp != 0)
        {
            return name_cmp;
        }
        return strcmp(machineID[left_index], machineID[right_index]);
    }

    if (left_empty)
    {
        return 1;
    }

    if (right_empty)
    {
        return -1;
    }

    int cmp = strcmp(left_date, right_date);
    if (cmp != 0)
    {
        return cmp;
    }

    cmp = strcmp(machineName[left_index], machineName[right_index]);
    if (cmp != 0)
    {
        return cmp;
    }

    return strcmp(machineID[left_index], machineID[right_index]);
}

static int string_contains_case_insensitive(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
    {
        return 0;
    }

    if (needle[0] == '\0')
    {
        return 1;
    }

    for (const char *h = haystack; *h; ++h)
    {
        size_t i = 0;
        while (needle[i])
        {
            unsigned char hc = (unsigned char)h[i];
            if (hc == '\0')
            {
                break;
            }

            unsigned char nc = (unsigned char)needle[i];
            if (tolower(hc) != tolower(nc))
            {
                break;
            }
            i++;
        }

        if (needle[i] == '\0')
        {
            return 1;
        }
    }

    return 0;
}

static int parse_machine_id_value(const char *id, int *out_value)
{
    if (!id || !out_value || id[0] == '\0')
    {
        return -1;
    }

    errno = 0;
    char *endptr = NULL;
    long value = strtol(id, &endptr, 10);
    if (errno != 0 || endptr == id || (endptr && *endptr != '\0') || value < 1 || value > INT_MAX)
    {
        return -1;
    }

    *out_value = (int)value;
    return 0;
}

static int normalize_machine_id_value(int value, char *buffer, size_t size)
{
    if (value < 1 || !buffer || size == 0)
    {
        return -1;
    }

    int written = snprintf(buffer, size, "%d", value);
    if (written < 0 || (size_t)written >= size)
    {
        return -1;
    }

    return 0;
}

static int is_machine_id_unique(const char *id)
{
    if (!id)
    {
        return 0;
    }

    for (int i = 0; i < record_count; ++i)
    {
        if (strcmp(machineID[i], id) == 0)
        {
            return 0;
        }
    }

    return 1;
}

static int find_smallest_available_machine_id(void)
{
    int limit = record_count + 1;
    if (limit < 1)
    {
        limit = 1;
    }

    /* Track usage for IDs within a reasonable bound. */
    char used[MAX_RECORDS + 2] = {0};

    for (int i = 0; i < record_count; ++i)
    {
        int value = 0;
        if (parse_machine_id_value(machineID[i], &value) == 0 && value >= 1 && value <= limit)
        {
            used[value] = 1;
        }
    }

    for (int candidate = 1; candidate <= limit; ++candidate)
    {
        if (!used[candidate])
        {
            return candidate;
        }
    }

    return limit + 1;
}

static int prompt_machine_id(char *buffer, size_t size)
{
    if (!buffer || size == 0)
    {
        return INPUT_ERROR;
    }

    while (1)
    {
        int next_id = find_smallest_available_machine_id();
        if (buffer[0] != '\0')
        {
            printf("Current machine ID: %s\n", buffer);
        }

        char prompt[128];
        snprintf(prompt, sizeof(prompt),
                 "Enter machine ID (leave blank for auto %d): ",
                 next_id);

        char input[MAX_ID];
        int status = safe_input(input, sizeof(input), prompt);
        if (status == INPUT_EXIT || status == INPUT_CANCELLED || status == INPUT_BACK || status == INPUT_ERROR)
        {
            return status;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (input[0] == '\0')
        {
            char normalized[MAX_ID];
            if (normalize_machine_id_value(next_id, normalized, sizeof(normalized)) != 0)
            {
                printf("Failed to assign machine ID automatically.\n");
                return INPUT_ERROR;
            }

            if (!is_machine_id_unique(normalized))
            {
                /* In theory this should not happen, but guard anyway. */
                printf("Automatically generated machine ID '%s' is already in use.\n", normalized);
                continue;
            }

            int written = snprintf(buffer, size, "%s", normalized);
            if (written < 0 || (size_t)written >= size)
            {
                printf("Failed to store machine ID.\n");
                return INPUT_ERROR;
            }

            printf("Assigned machine ID: %s\n", buffer);
            return INPUT_OK;
        }

        if (!is_valid_machine_id(input))
        {
            printf("Machine ID may only contain letters, numbers, '-', '_' or '.'.\n");
            continue;
        }

        char normalized[MAX_ID];
        const char *final_id = input;

        int value = 0;
        if (parse_machine_id_value(input, &value) == 0)
        {
            if (normalize_machine_id_value(value, normalized, sizeof(normalized)) != 0)
            {
                printf("Machine ID is invalid or too large.\n");
                continue;
            }
            final_id = normalized;
        }

        if (!is_machine_id_unique(final_id))
        {
            printf("Machine ID '%s' already exists.\n", final_id);
            continue;
        }

        int written = snprintf(buffer, size, "%s", final_id);
        if (written < 0 || (size_t)written >= size)
        {
            printf("Failed to store machine ID.\n");
            return INPUT_ERROR;
        }

        printf("Using machine ID: %s\n", buffer);
        return INPUT_OK;
    }
}

static int prompt_yes_no_common(const char *prompt, const char *invalid_message,
                                int allow_blank_default, int default_choice,
                                int error_default, int *out_choice)
{
    if (!out_choice)
        return INPUT_ERROR;

    for (;;)
    {
        char response[8];
        int status = safe_input(response, sizeof(response), prompt);

        if (status == INPUT_TOO_LONG)
        {
            if (invalid_message)
                printf("%s\n", invalid_message);
            continue;
        }
        if (status == INPUT_ERROR)
        {
            if (error_default >= 0)
            {
                *out_choice = error_default;
                return INPUT_OK;
            }
            return status;
        }
        if (status != INPUT_OK)
            return status;
        char choice = response[0];
        if (!choice)
        {
            if (allow_blank_default)
            {
                *out_choice = default_choice;
                return INPUT_OK;
            }
            if (invalid_message)
                printf("%s\n", invalid_message);
            continue;
        }
        if (choice == 'Y' || choice == 'y') { *out_choice = 1; return INPUT_OK; }
        if (choice == 'N' || choice == 'n') { *out_choice = 0; return INPUT_OK; }
        if (invalid_message)
            printf("%s\n", invalid_message);
    }
}

static int prompt_confirmation(const char *prompt, int *out_confirmed)
{
    return prompt_yes_no_common(prompt, "Please enter Y or N.", 1, 1, 1, out_confirmed);
}

static int copy_csv_file(const char *source, const char *destination)
{
    if (!source || !destination)
    {
        return -1;
    }

    FILE *src = fopen(source, "r");
    if (!src)
    {
        perror("Open source CSV");
        return -1;
    }

    FILE *dst = fopen(destination, "w");
    if (!dst)
    {
        perror("Create CSV copy");
        fclose(src);
        return -1;
    }

    char buffer[1024];
    size_t bytes_read;
    int error = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        if (fwrite(buffer, 1, bytes_read, dst) != bytes_read)
        {
            perror("Write CSV");
            error = 1;
            break;
        }
    }

    if (!error && ferror(src))
    {
        perror("Read CSV");
        error = 1;
    }

    if (fclose(dst) != 0)
    {
        perror("Close CSV after copy");
        error = 1;
    }

    if (fclose(src) != 0)
    {
        perror("Close source CSV");
        error = 1;
    }

    return error ? -1 : 0;
}

static int prompt_csv_index_selection(size_t csv_count, const char *prompt, int *out_index)
{
    if (csv_count == 0 || !prompt || !out_index)
    {
        return INPUT_ERROR;
    }

    char buffer[32];

    while (1)
    {
        int status = safe_input(buffer, sizeof(buffer), prompt);
        if (status == INPUT_EXIT)
        {
            return INPUT_EXIT;
        }
        if (status == INPUT_BACK || status == INPUT_CANCELLED)
        {
            return INPUT_CANCELLED;
        }

        if (status == INPUT_ERROR)
        {
            return INPUT_ERROR;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (buffer[0] == '\0')
        {
            printf("Please enter a number between 1 and %zu.\n", csv_count);
            continue;
        }

        char *endptr = NULL;
        long value = strtol(buffer, &endptr, 10);
        if (endptr && *endptr == '\0' && value >= 1 && (size_t)value <= csv_count)
        {
            *out_index = (int)(value - 1);
            return INPUT_OK;
        }

        printf("Please enter a number between 1 and %zu.\n", csv_count);
    }
}

static int read_csv_menu_choice(size_t csv_count, int *selected_index, char *command)
{
    if (!command)
    {
        return INPUT_ERROR;
    }

    char buffer[32];

    while (1)
    {
        int status = safe_input(buffer, sizeof(buffer), "Select CSV option : ");
        if (status == INPUT_EXIT)
        {
            return INPUT_EXIT;
        }
        if (status == INPUT_BACK || status == INPUT_CANCELLED)
        {
            return INPUT_CANCELLED;
        }

        if (status == INPUT_ERROR)
        {
            printf("Input error detected. Leaving CSV menu.\n");
            return INPUT_ERROR;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (buffer[0] == '\0')
        {
            if (csv_count > 0)
            {
                printf("Invalid choice. Enter a number between 1 and %zu or C/A/N/D/Q.\n", csv_count);
            }
            else
            {
                printf("Invalid choice. Enter A/N/Q.\n");
            }
            continue;
        }

        if (isalpha((unsigned char)buffer[0]) && buffer[1] == '\0')
        {
            char c = (char)toupper((unsigned char)buffer[0]);
            if (c == 'A' || c == 'N' || c == 'D' || c == 'Q' || c == 'C')
            {
                *command = c;
                if (selected_index)
                {
                    *selected_index = -1;
                }
                return INPUT_OK;
            }
        }

        char *endptr = NULL;
        long value = strtol(buffer, &endptr, 10);
        if (endptr && *endptr == '\0' && value >= 1 && (size_t)value <= csv_count)
        {
            if (selected_index)
            {
                *selected_index = (int)(value - 1);
            }
            *command = 'S';
            return INPUT_OK;
        }

        if (csv_count > 0)
        {
            printf("Invalid choice. Enter a number between 1 and %zu or C/A/N/D/Q.\n", csv_count);
        }
        else
        {
            printf("Invalid choice. Enter C/A/N/Q.\n");
        }
    }
}

static void render_csv_menu(char *const *csv_files, size_t csv_count)
{
    const char *current_path = maintenance_get_csv_path();
    const char *display_path = (current_path && current_path[0] != '\0') ? current_path : "default";

    printf("\nCSV File Management\n\n");
    printf("CSV current path: %s\n\n", display_path);

    if (csv_count > 0 && csv_files)
    {
        for (size_t i = 0; i < csv_count; ++i)
        {
            printf("%zu. %s\n", i + 1, csv_files[i]);
        }
        printf("\n");
    }
    else
    {
        printf("(No CSV files detected in current directory)\n\n");
    }

    printf("Other\n");
    printf("C. Continue with \"%s\" CSV file\n", display_path);
    printf("A. Enter CSV file path manually\n");
    printf("N. Create a new blank CSV file\n");
    if (csv_count > 0)
    {
        printf("D. Duplicate an existing CSV file\n");
    }
    printf("Q. Exit\n");
}

static int UNUSED_FN show_csv_control_menu(void)
{
    DIR *dir = opendir(".");
    char **csv_files = NULL;
    size_t csv_count = 0;
    size_t capacity = 0;

    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            if (!has_csv_extension(entry->d_name))
            {
                continue;
            }

            if (!is_regular_file(entry->d_name, entry))
            {
                continue;
            }

            size_t len = strlen(entry->d_name) + 1;
            if (len >= CSV_PATH_MAX)
            {
                continue;
            }

            if (csv_count == capacity)
            {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                char **tmp = realloc(csv_files, new_capacity * sizeof(char *));
                if (!tmp)
                {
                    break;
                }
                csv_files = tmp;
                capacity = new_capacity;
            }

            csv_files[csv_count] = malloc(len);
            if (!csv_files[csv_count])
            {
                break;
            }

            memcpy(csv_files[csv_count], entry->d_name, len);
            csv_count++;
        }

        closedir(dir);
    }
    else
    {
        perror("Open working directory");
    }

    if (csv_count > 1)
    {
        qsort(csv_files, csv_count, sizeof(char *), compare_csv_names);
    }

    int result = 1;
    int needs_refresh = 1;

    while (!exit_requested && !interrupt_requested)
    {
        if (needs_refresh)
        {
            clear_console_output();
            render_csv_menu(csv_files, csv_count);
            needs_refresh = 0;
        }

        int selected_index = -1;
        char command = '\0';
        int status = read_csv_menu_choice(csv_count, &selected_index, &command);
        if (status == INPUT_EXIT)
        {
            result = -1;
            break;
        }
        if (status == INPUT_CANCELLED)
        {
            needs_refresh = 1;
            continue;
        }

        if (status == INPUT_ERROR)
        {
            result = -1;
            break;
        }

        if (command == 'S' && selected_index >= 0 && (size_t)selected_index < csv_count)
        {
            const char *path = csv_files[selected_index];
            if (maintenance_set_csv_path(path) == 0)
            {
                printf("Using CSV file: %s\n", path);
                result = 0;
                break;
            }

            printf("Failed to set CSV path to '%s'.\n", path);
        }
        else if (command == 'A')
        {
            char path_buffer[CSV_PATH_MAX];
            int input_status = safe_input(path_buffer, sizeof(path_buffer), "Enter CSV file path: ");
            if (input_status == INPUT_EXIT)
            {
                result = -1;
                break;
            }
            if (input_status == INPUT_BACK || input_status == INPUT_CANCELLED)
            {
                needs_refresh = 1;
                continue;
            }

            if (input_status == INPUT_ERROR)
            {
                printf("Input error detected. Leaving CSV menu.\n");
                result = -1;
                break;
            }

            if (input_status == INPUT_TOO_LONG || path_buffer[0] == '\0')
            {
                printf("Path cannot be empty.\n");
                continue;
            }

            if (maintenance_set_csv_path(path_buffer) != 0)
            {
                printf("Failed to set CSV path to '%s'.\n", path_buffer);
                continue;
            }

            printf("Using CSV file: %s\n", path_buffer);
            result = 0;
            break;
        }
        else if (command == 'N')
        {
            char filename[CSV_PATH_MAX];
            int input_status = safe_input(filename, sizeof(filename), "Enter name for new CSV file: ");
            if (input_status == INPUT_EXIT)
            {
                result = -1;
                break;
            }
            if (input_status == INPUT_BACK || input_status == INPUT_CANCELLED)
            {
                needs_refresh = 1;
                continue;
            }

            if (input_status == INPUT_ERROR)
            {
                printf("Input error detected. Leaving CSV menu.\n");
                result = -1;
                break;
            }

            if (input_status == INPUT_TOO_LONG || filename[0] == '\0')
            {
                printf("Filename cannot be empty.\n");
                continue;
            }

            if (write_blank_csv(filename) != 0)
            {
                printf("Failed to create new CSV file '%s'.\n", filename);
                continue;
            }

            if (maintenance_set_csv_path(filename) != 0)
            {
                printf("Failed to set CSV path to '%s'.\n", filename);
                continue;
            }

            printf("Created new CSV file: %s\n", filename);
            reload_records_with_warning();
            result = 0;
            break;
        }
        else if (command == 'D')
        {
            if (csv_count == 0)
            {
                printf("No CSV files available to duplicate.\n");
                continue;
            }

            int source_index = -1;
            int index_status = prompt_csv_index_selection(csv_count, "Enter the number of the CSV to duplicate:", &source_index);
            if (index_status == INPUT_EXIT)
            {
                result = -1;
                break;
            }
            if (index_status == INPUT_CANCELLED)
            {
                needs_refresh = 1;
                continue;
            }

            if (index_status == INPUT_ERROR)
            {
                printf("Input error detected. Leaving CSV menu.\n");
                result = -1;
                break;
            }

            char destination[CSV_PATH_MAX];
            int dest_status = safe_input(destination, sizeof(destination), "Enter name for the new CSV file: ");
            if (dest_status == INPUT_EXIT)
            {
                result = -1;
                break;
            }
            if (dest_status == INPUT_BACK || dest_status == INPUT_CANCELLED)
            {
                needs_refresh = 1;
                continue;
            }

            if (dest_status == INPUT_ERROR)
            {
                printf("Input error detected. Leaving CSV menu.\n");
                result = -1;
                break;
            }

            if (dest_status == INPUT_TOO_LONG || destination[0] == '\0')
            {
                printf("Filename cannot be empty.\n");
                continue;
            }

            if (copy_csv_file(csv_files[source_index], destination) != 0)
            {
                printf("Failed to duplicate '%s' to '%s'.\n", csv_files[source_index], destination);
                continue;
            }

            if (maintenance_set_csv_path(destination) != 0)
            {
                printf("Failed to set CSV path to '%s'.\n", destination);
                continue;
            }

            printf("Duplicated '%s' to '%s'.\n", csv_files[source_index], destination);
            reload_records_with_warning();
            result = 0;
            break;
        }
        else if (command == 'C')
        {
            printf("Continuing with the current CSV file.\n");
            result = 1;
            break;
        }
        else if (command == 'Q')
        {
            // exit program
            result = -1;
            break;
        }
    }

    for (size_t i = 0; i < csv_count; ++i)
    {
        free(csv_files[i]);
    }
    free(csv_files);

    if (exit_requested)
    {
        return -1;
    }

    if (interrupt_requested)
    {
        return -1;
    }
    clear_console_output();
    return result;
}

static void UNUSED_FN handle_addon_menu(void)
{
    while (!exit_requested && !interrupt_requested)
    {
        display_addon_menu();
        int choice = read_addon_choice();
        if (choice <= 0)
        {
            return;
        }

        if (choice == 1)
        {
            resequence_machine_ids();
        }
        else if (choice == 2)
        {
            return;
        }
    }
}

static void display_addon_menu(void)
{
    printf("\nAdd-on Tools\n");
    printf("1. Resequence machine IDs by maintenance date\n");
    printf("2. Return to main menu\n");
}

static int read_addon_choice(void)
{
    char buffer[16];

    while (1)
    {
        int status = safe_input(buffer, sizeof(buffer), "Select add-on option (Ctrl+X cancel, Ctrl+Z back): ");
        if (status == INPUT_EXIT)
        {
            return -1;
        }

        if (status == INPUT_BACK || status == INPUT_CANCELLED)
        {
            print_cancel_message();
            return 0;
        }

        if (status == INPUT_ERROR)
        {
            printf("Input error detected. Returning to main menu.\n");
            return -1;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (buffer[0] == '\0')
        {
            printf("Invalid choice. Enter 1 or 2.\n");
            continue;
        }

        char *endptr = NULL;
        long value = strtol(buffer, &endptr, 10);
        if (endptr && *endptr == '\0' && value >= 1 && value <= 2)
        {
            return (int)value;
        }

        printf("Invalid choice. Enter 1 or 2.\n");
    }
}

static void resequence_machine_ids(void)
{
    if (record_count <= 0)
    {
        printf("No maintenance records available to resequence.\n");
        return;
    }

    int indices[MAX_RECORDS];
    for (int i = 0; i < record_count; ++i)
    {
        indices[i] = i;
    }

    qsort(indices, record_count, sizeof(int), compare_records_by_schedule);

    char new_names[MAX_RECORDS][MAX_NAME];
    char new_dates[MAX_RECORDS][MAX_DATE];
    char new_details[MAX_RECORDS][MAX_DETAILS];

    for (int pos = 0; pos < record_count; ++pos)
    {
        int src = indices[pos];
        snprintf(new_names[pos], MAX_NAME, "%.*s", MAX_NAME - 1, machineName[src]);
        snprintf(new_dates[pos], MAX_DATE, "%.*s", MAX_DATE - 1, maintenanceDate[src]);
        snprintf(new_details[pos], MAX_DETAILS, "%.*s", MAX_DETAILS - 1, maintenanceDetails[src]);
    }

    for (int i = 0; i < record_count; ++i)
    {
        snprintf(machineName[i], MAX_NAME, "%.*s", MAX_NAME - 1, new_names[i]);
        snprintf(maintenanceDate[i], MAX_DATE, "%.*s", MAX_DATE - 1, new_dates[i]);
        snprintf(maintenanceDetails[i], MAX_DETAILS, "%.*s", MAX_DETAILS - 1, new_details[i]);

        int written = snprintf(machineID[i], MAX_ID, "%d", i + 1);
        if (written < 0 || written >= MAX_ID)
        {
            printf("Warning: Failed to assign machine ID for record %d.\n", i + 1);
        }
    }

    if (save_all_records() == 0)
    {
        if (reload_records_with_warning() == 0)
        {
            printf("Resequenced machine IDs by maintenance date and saved changes.\n");
        }
    }
    else
    {
        printf("Resequenced machine IDs in memory, but saving the file failed.\n");
    }
}

static void print_record_table_header(void)
{
    /* Top border */
    printf("+----------------------+--------------+--------------+--------------------------------+\n");
    /* Header titles */
    printf("| Machine Name         | Machine ID   | Date         | Details                        |\n");
    /* Header separator */
    printf("+----------------------+--------------+--------------+--------------------------------+\n");
}

static void print_record_table_row(int index)
{
    printf("| %-20.20s | %-12.12s | %-12.12s | %-30.30s |\n",
           machineName[index],
           machineID[index],
           maintenanceDate[index],
           maintenanceDetails[index]);
}

static void print_record_table_footer(void)
{
    printf("+----------------------+--------------+--------------+--------------------------------+\n");
}

static void print_record_preview_from_values(const char *name, const char *id,
                                             const char *date, const char *details)
{
    const char *safe_name = name ? name : "";
    const char *safe_id = id ? id : "";
    const char *safe_date = date ? date : "";
    const char *safe_details = details ? details : "";

    print_record_table_header();
    printf("| %-20.20s | %-12.12s | %-12.12s | %-30.30s |\n",
           safe_name,
           safe_id,
           safe_date,
           safe_details);
    print_record_table_footer();
}

static int get_terminal_height(void)
{
#if defined(_WIN32)
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != INVALID_HANDLE_VALUE)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(output, &info))
        {
            int height = info.srWindow.Bottom - info.srWindow.Top + 1;
            if (height > 0)
            {
                return height;
            }
        }
    }
#else
    if (isatty(STDOUT_FILENO))
    {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0)
        {
            return (int)ws.ws_row;
        }
    }
#endif

    const char *env_lines = getenv("LINES");
    if (env_lines)
    {
        char *endptr = NULL;
        long value = strtol(env_lines, &endptr, 10);
        if (endptr && *endptr == '\0' && value > 0 && value <= INT_MAX)
        {
            return (int)value;
        }
    }

    return 24;
}

static int calculate_records_per_page(void)
{
    int terminal_height = get_terminal_height();
    const int reserved_lines = 8;
    int available_rows = terminal_height - reserved_lines;

    if (available_rows < 1)
    {
        available_rows = 1;
    }

    if (record_count > 0 && available_rows > record_count)
    {
        available_rows = record_count;
    }

    return (available_rows > 0) ? available_rows : 1;
}

static void clear_console_output(void)
{
#if defined(UNIT_TEST)
    (void)0;
#elif defined(_WIN32)
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == INVALID_HANDLE_VALUE)
    {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(output, &info))
    {
        return;
    }

    DWORD cell_count = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
    COORD home = {0, 0};
    DWORD written = 0;

    FillConsoleOutputCharacter(output, ' ', cell_count, home, &written);
    FillConsoleOutputAttribute(output, info.wAttributes, cell_count, home, &written);
    SetConsoleCursorPosition(output, home);
#else
    if (!isatty(STDOUT_FILENO))
    {
        return;
    }

    printf("\033[2J\033[H");
    fflush(stdout);
#endif
}

#ifndef UNIT_TEST
static void UNUSED_FN clear_function_log(void)
{
    if (exit_requested || interrupt_requested)
    {
        return;
    }

    char buffer[8];
    printf("\n");

    while (1)
    {
        int status = safe_input(buffer, sizeof(buffer), "Press Enter to clear log and return to the menu: ");

        if (status == INPUT_EXIT)
        {
            return;
        }

        if (status == INPUT_CANCELLED || status == INPUT_BACK)
        {
            clear_console_output();
            return;
        }

        if (status == INPUT_ERROR)
        {
            return;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        break;
    }

    clear_console_output();
}
#endif

static int interpret_paging_arrow(const char *input)
{
    if (!input || input[0] != 0x1B)
    {
        return 0;
    }

    size_t len = strlen(input);
    if (len < 2)
    {
        return 0;
    }

    unsigned char second = (unsigned char)input[1];
    if (second != '[' && second != 'O')
    {
        return 0;
    }

    char final = (len > 0) ? input[len - 1] : '\0';
    if (final == 'C' || final == 'B')
    {
        return 1;
    }
    if (final == 'D' || final == 'A')
    {
        return -1;
    }

    return 0;
}

static int read_paging_command(char *buffer, size_t size)
{
    if (!buffer || size <= 0)
    {
        return INPUT_ERROR;
    }

#if defined(UNIT_TEST)
    return safe_input(buffer, size, "Paging command: ");
#elif defined(_WIN32)
    return safe_input(buffer, size, "Paging command: ");
#else
    if (!isatty(STDIN_FILENO))
    {
        return safe_input(buffer, size, "Paging command: ");
    }

    struct termios original;
    if (tcgetattr(STDIN_FILENO, &original) != 0)
    {
        return safe_input(buffer, size, "Paging command: ");
    }

    struct termios raw = original;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
    {
        return safe_input(buffer, size, "Paging command: ");
    }

    printf("Paging command: ");
    fflush(stdout);

    int status = INPUT_OK;
    size_t pos = 0;
    int reading_number = 0;

    while (pos + 1 < (size_t)size)
    {
        unsigned char ch = 0;
        ssize_t bytes_read = read(STDIN_FILENO, &ch, 1);
        if (bytes_read <= 0)
        {
            status = INPUT_ERROR;
            pos = 0;
            buffer[0] = '\0';
            break;
        }

        if (ch == 0x18)
        {
            status = INPUT_CANCELLED;
            pos = 0;
            buffer[0] = '\0';
            break;
        }

        if (ch == 0x1A)
        {
            status = INPUT_BACK;
            pos = 0;
            buffer[0] = '\0';
            break;
        }

        if (ch == 0x03)
        {
            request_exit();
            status = INPUT_EXIT;
            pos = 0;
            buffer[0] = '\0';
            break;
        }

        if (ch == '\r' || ch == '\n')
        {
            if (!reading_number)
            {
                pos = 0;
                buffer[0] = '\0';
            }
            break;
        }

        if (ch == 0x1B)
        {
            buffer[pos++] = (char)ch;

            while (pos + 1 < (size_t)size)
            {
                unsigned char next = 0;
                ssize_t more_read = read(STDIN_FILENO, &next, 1);
                if (more_read <= 0)
                {
                    status = INPUT_ERROR;
                    pos = 0;
                    buffer[0] = '\0';
                    break;
                }

                buffer[pos++] = (char)next;

                if ((next >= 'A' && next <= 'D') || next == '~')
                {
                    break;
                }

                if (!isdigit((unsigned char)next) && next != ';' && next != '[' && next != 'O')
                {
                    break;
                }
            }
            break;
        }

        if (pos + 1 >= (size_t)size)
        {
            status = INPUT_TOO_LONG;
            pos = 0;
            buffer[0] = '\0';
            break;
        }

        buffer[pos++] = (char)ch;
        buffer[pos] = '\0';

        if (!reading_number)
        {
            if (isdigit((unsigned char)ch))
            {
                reading_number = 1;
                continue;
            }
            break;
        }
    }

    buffer[pos] = '\0';

    if (status == INPUT_TOO_LONG)
    {
        tcflush(STDIN_FILENO, TCIFLUSH);
    }

    printf("\n");
    fflush(stdout);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &original) != 0)
    {
        status = INPUT_ERROR;
    }

    return status;
#endif
}

int reload_records_with_warning(void)
{
    if (load_records() != 0)
    {
        const char *path = maintenance_get_csv_path();
        if (!path || path[0] == '\0')
        {
            path = "(unknown)";
        }

        printf("Warning: Failed to reload records from '%s'. Data may be stale.\n", path);
        return -1;
    }

    return 0;
}

const char *maintenance_get_csv_path(void)
{
    return csv_file_path;
}

int maintenance_set_csv_path(const char *path)
{
    if (!path || path[0] == '\0')
    {
        return -1;
    }

    size_t len = strlen(path);
    if (len >= CSV_PATH_MAX)
    {
        return -1;
    }

    int written = snprintf(csv_file_path, sizeof(csv_file_path), "%s", path);
    if (written < 0 || (size_t)written >= sizeof(csv_file_path))
    {
        // Truncation occurred
        return -1;
    }

    return 0;
}

#if defined(ENABLE_INTERNAL_TESTS)

#if defined(_WIN32)
#define maintenance_dup _dup
#define maintenance_dup2 _dup2
#define maintenance_close _close
#define maintenance_fileno _fileno
#else
#define maintenance_dup dup
#define maintenance_dup2 dup2
#define maintenance_close close
#define maintenance_fileno fileno
#endif

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

    *saved_fd = maintenance_dup(maintenance_fileno(stdin));
    if (*saved_fd < 0)
    {
        return -1;
    }

    *temp_file = tmpfile();
    if (!*temp_file)
    {
        maintenance_close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    fputs(text, *temp_file);
    rewind(*temp_file);

    if (maintenance_dup2(maintenance_fileno(*temp_file), maintenance_fileno(stdin)) != 0)
    {
        fclose(*temp_file);
        *temp_file = NULL;
        maintenance_close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    return 0;
}

static void restore_stdin(int saved_fd, FILE *temp_file)
{
    if (saved_fd >= 0)
    {
        maintenance_dup2(saved_fd, maintenance_fileno(stdin));
        maintenance_close(saved_fd);
    }

    if (temp_file)
    {
        fclose(temp_file);
    }

    clearerr(stdin);
}

static int redirect_stdout_to_temp(int *saved_fd, FILE **temp_file)
{
    if (!saved_fd || !temp_file)
    {
        return -1;
    }

    *saved_fd = maintenance_dup(maintenance_fileno(stdout));
    if (*saved_fd < 0)
    {
        return -1;
    }

    *temp_file = tmpfile();
    if (!*temp_file)
    {
        maintenance_close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    fflush(stdout);

    if (maintenance_dup2(maintenance_fileno(*temp_file), maintenance_fileno(stdout)) == -1)
    {
        fclose(*temp_file);
        *temp_file = NULL;
        maintenance_close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    return 0;
}

static int read_captured_stdout(FILE *temp_file, char *buffer, size_t size)
{
    if (!temp_file || !buffer || size == 0)
    {
        return -1;
    }

    fflush(stdout);
    fflush(temp_file);

    long original = ftell(temp_file);
    if (original < 0)
    {
        original = 0;
    }

    rewind(temp_file);

    size_t read = fread(buffer, 1, size - 1, temp_file);
    buffer[read] = '\0';

    fseek(temp_file, original, SEEK_SET);

    return 0;
}

static void restore_stdout(int saved_fd, FILE *temp_file)
{
    fflush(stdout);

    if (saved_fd >= 0)
    {
        maintenance_dup2(saved_fd, maintenance_fileno(stdout));
        maintenance_close(saved_fd);
    }

    if (temp_file)
    {
        fclose(temp_file);
    }
}

/* -------------------------- Unit test harness -------------------------- */

typedef struct
{
    const char *suite;
    const char *name;
    int passed;
    char detail[160];
} unit_test_result_t;

static unit_test_result_t unit_results[128];
static size_t unit_result_count = 0;

static void unit_record_result(const char *suite,
                               const char *name,
                               int passed,
                               const char *fmt,
                               ...)
{
    if (unit_result_count >= (sizeof(unit_results) / sizeof(unit_results[0])))
    {
        return;
    }

    unit_test_result_t *entry = &unit_results[unit_result_count++];
    entry->suite = suite;
    entry->name = name;
    entry->passed = passed;

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->detail, sizeof(entry->detail), fmt, args);
    va_end(args);
}

#define record_result unit_record_result
#define ASSERT_TRUE(SUITE, NAME, EXPR, FMT, ...)                                                   \
    do                                                                                             \
    {                                                                                              \
        int _ok = (EXPR);                                                                          \
        record_result((SUITE), (NAME), _ok, (FMT), ##__VA_ARGS__);                                 \
    } while (0)

#define ASSERT_EQ_INT(SUITE, NAME, EXPECTED, ACTUAL)                                               \
    do                                                                                             \
    {                                                                                              \
        int _exp = (EXPECTED);                                                                     \
        int _act = (ACTUAL);                                                                       \
        int _ok = (_exp == _act);                                                                  \
        record_result((SUITE), (NAME), _ok, "expected %d, actual %d", _exp, _act);                 \
    } while (0)

#define ASSERT_STREQ(SUITE, NAME, EXPECTED, ACTUAL)                                                \
    do                                                                                             \
    {                                                                                              \
        const char *_exp = (EXPECTED);                                                             \
        const char *_act = (ACTUAL);                                                               \
        int _ok = ((_exp == NULL && _act == NULL) ||                                               \
                    (_exp != NULL && _act != NULL && strcmp(_exp, _act) == 0));                    \
        record_result((SUITE), (NAME), _ok, "expected '%s', actual '%s'",                          \
                       _exp ? _exp : "(null)", _act ? _act : "(null)");                         \
    } while (0)

static void print_unit_results_table(void)
{
    size_t passed = 0;
    for (size_t i = 0; i < unit_result_count; ++i)
    {
        const unit_test_result_t *row = &unit_results[i];
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

    printf("Total: %zu, Passed: %zu, Failed: %zu\n",
           unit_result_count, passed, unit_result_count - passed);
    puts("-------");
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

static void test_display_records_outputs_content(void)
{
    const char *suite = "Main Actions";
    reset_storage();
    exit_requested = 0;
    interrupt_requested = 0;

    snprintf(machineName[0], MAX_NAME, "%s", "Hydraulic Press");
    snprintf(machineID[0], MAX_ID, "%s", "1");
    snprintf(maintenanceDate[0], MAX_DATE, "%s", "2025-08-01");
    snprintf(maintenanceDetails[0], MAX_DETAILS, "%s", "Changed hydraulic fluid");
    record_count = 1;

    int saved_stdout = -1;
    FILE *stdout_temp = NULL;
    int capture_status = redirect_stdout_to_temp(&saved_stdout, &stdout_temp);
    ASSERT_TRUE(suite, "display stdout redirect", capture_status == 0,
                "redirect status %d", capture_status);

    if (capture_status == 0)
    {
        display_records();

        char output[1024];
        int read_status = read_captured_stdout(stdout_temp, output, sizeof(output));
        ASSERT_TRUE(suite, "display capture readable", read_status == 0,
                    "read status %d", read_status);
        if (read_status == 0)
        {
            ASSERT_TRUE(suite, "display lists machine", strstr(output, "Hydraulic Press") != NULL,
                        "output: %s", output);
        }
    }

    restore_stdout(saved_stdout, stdout_temp);
    reset_storage();
}

static void test_add_record_interactive_success(void)
{
    const char *suite = "Main Actions";
    reset_storage();
    exit_requested = 0;
    interrupt_requested = 0;

    const char *input = "Hydraulic Press\n\n2025-10-10\nRoutine maintenance\nY\n";
    int saved_stdin = -1;
    FILE *stdin_temp = NULL;
    int stdin_status = redirect_stdin_from_string(input, &saved_stdin, &stdin_temp);
    ASSERT_TRUE(suite, "add record stdin redirect", stdin_status == 0,
                "redirect status %d", stdin_status);

    int saved_stdout = -1;
    FILE *stdout_temp = NULL;
    int stdout_status = redirect_stdout_to_temp(&saved_stdout, &stdout_temp);
    ASSERT_TRUE(suite, "add record stdout redirect", stdout_status == 0,
                "redirect status %d", stdout_status);

    if (stdin_status == 0)
    {
        add_record();
    }

    if (stdout_status == 0)
    {
        char output[1024];
        int read_status = read_captured_stdout(stdout_temp, output, sizeof(output));
        ASSERT_TRUE(suite, "add record capture readable", read_status == 0,
                    "read status %d", read_status);
        if (read_status == 0)
        {
            ASSERT_TRUE(suite, "add record confirmation", strstr(output, "Record added") != NULL,
                        "output: %s", output);
        }
    }

    restore_stdout(saved_stdout, stdout_temp);
    restore_stdin(saved_stdin, stdin_temp);

    ASSERT_EQ_INT(suite, "record count after add", 1, record_count);
    ASSERT_STREQ(suite, "stored name after add", "Hydraulic Press", machineName[0]);
    ASSERT_STREQ(suite, "stored id after add", "1", machineID[0]);
    ASSERT_STREQ(suite, "stored date after add", "2025-10-10", maintenanceDate[0]);
    ASSERT_STREQ(suite, "stored details after add", "Routine maintenance", maintenanceDetails[0]);

    reset_storage();
}

static void test_search_records_finds_matches(void)
{
    const char *suite = "Main Actions";
    reset_storage();
    exit_requested = 0;
    interrupt_requested = 0;

    snprintf(machineName[0], MAX_NAME, "%s", "Hydraulic Press");
    snprintf(machineID[0], MAX_ID, "%s", "1");
    snprintf(maintenanceDate[0], MAX_DATE, "%s", "2025-08-01");
    snprintf(maintenanceDetails[0], MAX_DETAILS, "%s", "Changed hydraulic fluid");

    snprintf(machineName[1], MAX_NAME, "%s", "Metal Lathe");
    snprintf(machineID[1], MAX_ID, "%s", "2");
    snprintf(maintenanceDate[1], MAX_DATE, "%s", "2025-07-15");
    snprintf(maintenanceDetails[1], MAX_DETAILS, "%s", "Replaced belts");
    record_count = 2;

    const char *input = "Lathe\n";
    int saved_stdin = -1;
    FILE *stdin_temp = NULL;
    int stdin_status = redirect_stdin_from_string(input, &saved_stdin, &stdin_temp);
    ASSERT_TRUE(suite, "search stdin redirect", stdin_status == 0,
                "redirect status %d", stdin_status);

    int saved_stdout = -1;
    FILE *stdout_temp = NULL;
    int stdout_status = redirect_stdout_to_temp(&saved_stdout, &stdout_temp);
    ASSERT_TRUE(suite, "search stdout redirect", stdout_status == 0,
                "redirect status %d", stdout_status);

    if (stdin_status == 0)
    {
        search_records();
    }

    if (stdout_status == 0)
    {
        char output[1024];
        int read_status = read_captured_stdout(stdout_temp, output, sizeof(output));
        ASSERT_TRUE(suite, "search capture readable", read_status == 0,
                    "read status %d", read_status);
        if (read_status == 0)
        {
            ASSERT_TRUE(suite, "search lists lathe", strstr(output, "Metal Lathe") != NULL,
                        "output: %s", output);
        }
    }

    restore_stdout(saved_stdout, stdout_temp);
    restore_stdin(saved_stdin, stdin_temp);

    reset_storage();
}

static void test_update_record_applies_changes(void)
{
    const char *suite = "Main Actions";
    reset_storage();
    exit_requested = 0;
    interrupt_requested = 0;

    snprintf(machineName[0], MAX_NAME, "%s", "Metal Lathe");
    snprintf(machineID[0], MAX_ID, "%s", "1");
    snprintf(maintenanceDate[0], MAX_DATE, "%s", "2025-07-15");
    snprintf(maintenanceDetails[0], MAX_DETAILS, "%s", "Replaced belts");
    record_count = 1;

    const char *input = "1\nNew Lathe\n2025-11-01\nUpdated details\nY\n";
    int saved_stdin = -1;
    FILE *stdin_temp = NULL;
    int stdin_status = redirect_stdin_from_string(input, &saved_stdin, &stdin_temp);
    ASSERT_TRUE(suite, "update stdin redirect", stdin_status == 0,
                "redirect status %d", stdin_status);

    int saved_stdout = -1;
    FILE *stdout_temp = NULL;
    int stdout_status = redirect_stdout_to_temp(&saved_stdout, &stdout_temp);
    ASSERT_TRUE(suite, "update stdout redirect", stdout_status == 0,
                "redirect status %d", stdout_status);

    if (stdin_status == 0)
    {
        update_record();
    }

    restore_stdout(saved_stdout, stdout_temp);
    restore_stdin(saved_stdin, stdin_temp);

    ASSERT_EQ_INT(suite, "record count after update", 1, record_count);
    ASSERT_STREQ(suite, "updated name", "New Lathe", machineName[0]);
    ASSERT_STREQ(suite, "updated date", "2025-11-01", maintenanceDate[0]);
    ASSERT_STREQ(suite, "updated details", "Updated details", maintenanceDetails[0]);

    reset_storage();
}

static void test_delete_record_removes_entry(void)
{
    const char *suite = "Main Actions";
    reset_storage();
    exit_requested = 0;
    interrupt_requested = 0;

    snprintf(machineName[0], MAX_NAME, "%s", "Metal Lathe");
    snprintf(machineID[0], MAX_ID, "%s", "1");
    snprintf(maintenanceDate[0], MAX_DATE, "%s", "2025-07-15");
    snprintf(maintenanceDetails[0], MAX_DETAILS, "%s", "Replaced belts");
    record_count = 1;

    const char *input = "1\nY\n";
    int saved_stdin = -1;
    FILE *stdin_temp = NULL;
    int stdin_status = redirect_stdin_from_string(input, &saved_stdin, &stdin_temp);
    ASSERT_TRUE(suite, "delete stdin redirect", stdin_status == 0,
                "redirect status %d", stdin_status);

    int saved_stdout = -1;
    FILE *stdout_temp = NULL;
    int stdout_status = redirect_stdout_to_temp(&saved_stdout, &stdout_temp);
    ASSERT_TRUE(suite, "delete stdout redirect", stdout_status == 0,
                "redirect status %d", stdout_status);

    if (stdin_status == 0)
    {
        delete_record();
    }

    restore_stdout(saved_stdout, stdout_temp);
    restore_stdin(saved_stdin, stdin_temp);

    ASSERT_EQ_INT(suite, "record count after delete", 0, record_count);

    reset_storage();
}

static int run_internal_unit_tests(void)
{
    unit_result_count = 0;

    test_csv_path_management();
    test_ensure_csv_exists_blank();
    test_save_and_load_round_trip();
    test_storage_capacity_guard();
    test_display_records_outputs_content();
    test_add_record_interactive_success();
    test_search_records_finds_matches();
    test_update_record_applies_changes();
    test_delete_record_removes_entry();

    print_unit_results_table();

    size_t failures = 0;
    for (size_t i = 0; i < unit_result_count; ++i)
    {
        if (!unit_results[i].passed)
        {
            ++failures;
        }
    }

    return failures == 0 ? 0 : 1;
}

#undef ASSERT_STREQ
#undef ASSERT_EQ_INT
#undef ASSERT_TRUE
#undef record_result

/* ----------------------- End-to-end test harness ----------------------- */

typedef struct
{
    const char *scenario;
    const char *step;
    int passed;
    char detail[256];
} e2e_result_t;

static e2e_result_t e2e_results[512];
static size_t e2e_result_count = 0;

static void e2e_record_result(const char *scenario,
                              const char *step,
                              int passed,
                              const char *fmt,
                              ...)
{
    if (e2e_result_count >= (sizeof(e2e_results) / sizeof(e2e_results[0])))
    {
        return;
    }

    e2e_result_t *entry = &e2e_results[e2e_result_count++];
    entry->scenario = scenario;
    entry->step = step;
    entry->passed = passed;

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->detail, sizeof(entry->detail), fmt, args);
    va_end(args);
}

#define record_result e2e_record_result
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
                       _exp ? _exp : "(null)", _act ? _act : "(null)");                          \
    } while (0)

static void print_e2e_results_table(void)
{
    size_t passed = 0;
    for (size_t i = 0; i < e2e_result_count; ++i)
    {
        const e2e_result_t *row = &e2e_results[i];
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
            e2e_result_count, passed, e2e_result_count - passed);
    fprintf(stderr, "-------\n");
}

static int feed_input(const char *script, FILE **out_temp)
{
    if (!script)
    {
        return -1;
    }

    int saved_fd = maintenance_dup(maintenance_fileno(stdin));
    if (saved_fd < 0)
    {
        return -1;
    }

    FILE *temp = tmpfile();
    if (!temp)
    {
        maintenance_close(saved_fd);
        return -1;
    }

    fputs(script, temp);
    rewind(temp);

    if (maintenance_dup2(maintenance_fileno(temp), maintenance_fileno(stdin)) != 0)
    {
        fclose(temp);
        maintenance_close(saved_fd);
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

    *saved_fd = maintenance_dup(maintenance_fileno(stdout));
    if (*saved_fd < 0)
    {
        return -1;
    }

    *capture_file = tmpfile();
    if (!*capture_file)
    {
        maintenance_close(*saved_fd);
        *saved_fd = -1;
        return -1;
    }

    if (maintenance_dup2(maintenance_fileno(*capture_file), maintenance_fileno(stdout)) != 0)
    {
        fclose(*capture_file);
        *capture_file = NULL;
        maintenance_close(*saved_fd);
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
        maintenance_dup2(saved_fd, maintenance_fileno(stdout));
        maintenance_close(saved_fd);
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

static void scenario_data_persistence_and_search(void)
{
    const char *scenario = "Persistence & Search";
    char original[CSV_PATH_MAX];
    snprintf(original, sizeof(original), "%s", maintenance_get_csv_path());

    const char *path = "tests/e2e-search.csv";
    FILE *f = fopen(path, "w");
    if (f)
    {
        fprintf(f, "MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n");
        fprintf(f, "Precision Lathe,LT-10,2024-01-15,Align headstock\n");
        fprintf(f, "Hydraulic Press,HP-99,2024-02-20,Replace seals\n");
        fprintf(f, "Laser Cutter,LC-07,2024-03-05,Clean optics\n");
        fclose(f);
    }

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

    const char *search_inputs[] = {"Lathe\n", "LC-07\n", "Unknown\n"};
    for (int i = 0; i < 3; ++i)
    {
        FILE *input_temp = NULL;
        int saved_fd = feed_input(search_inputs[i], &input_temp);
        SCENARIO_ASSERT_TRUE(scenario, "inject search input", saved_fd >= 0,
                              "prepared stdin for search %d", i + 1);

        if (saved_fd >= 0)
        {
            FILE *output_capture = NULL;
            int saved_stdout = -1;
            start_stdout_capture(&output_capture, &saved_stdout);
            search_records();
            discard_and_free(stop_stdout_capture(output_capture, saved_stdout));
        }

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
    SCENARIO_ASSERT_TRUE(scenario, "inject update input", saved_fd >= 0,
                          "prepared stdin for update");
    if (saved_fd >= 0)
    {
        output_capture = NULL;
        saved_stdout = -1;
        start_stdout_capture(&output_capture, &saved_stdout);
        update_record();
        discard_and_free(stop_stdout_capture(output_capture, saved_stdout));
    }
    restore_stdin(saved_fd, input_temp);

    SCENARIO_ASSERT_STREQ(scenario, "updated name", "New Laser Cutter", machineName[0]);
    SCENARIO_ASSERT_STREQ(scenario, "updated date", "2025-09-01", maintenanceDate[0]);

    save_status = save_all_records();
    SCENARIO_ASSERT_TRUE(scenario, "save after update", save_status == 0,
                          "save_all_records returned %d", save_status);

    input_temp = NULL;
    saved_fd = feed_input("1\ny\n", &input_temp);
    SCENARIO_ASSERT_TRUE(scenario, "inject delete input", saved_fd >= 0,
                          "prepared stdin for delete");
    if (saved_fd >= 0)
    {
        output_capture = NULL;
        saved_stdout = -1;
        start_stdout_capture(&output_capture, &saved_stdout);
        delete_record();
        discard_and_free(stop_stdout_capture(output_capture, saved_stdout));
    }
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

static int run_internal_e2e_tests(void)
{
    e2e_result_count = 0;

    scenario_complete_workflow();
    scenario_data_persistence_and_search();
    scenario_update_and_delete();

    print_e2e_results_table();

    size_t failures = 0;
    for (size_t i = 0; i < e2e_result_count; ++i)
    {
        if (!e2e_results[i].passed)
        {
            ++failures;
        }
    }

    return failures == 0 ? 0 : 1;
}

#undef SCENARIO_ASSERT_STREQ
#undef SCENARIO_ASSERT_EQ_INT
#undef SCENARIO_ASSERT_TRUE
#undef record_result

#undef maintenance_fileno
#undef maintenance_close
#undef maintenance_dup2
#undef maintenance_dup

#endif /* ENABLE_INTERNAL_TESTS */

#ifndef UNIT_TEST
void display_menu(void);
int read_menu_choice(void);

int main(int argc, char *argv[])
{
    int choice = 0;

#if defined(ENABLE_INTERNAL_TESTS)
    if (argc > 1)
    {
        if (strcmp(argv[1], "--run-unit-tests") == 0)
        {
            int status = run_unit_test_suite();
            return exit_program(status == 0 ? 0 : 1);
        }
        if (strcmp(argv[1], "--run-e2e-tests") == 0)
        {
            int status = run_end_to_end_suite();
            return exit_program(status == 0 ? 0 : 1);
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

#if !defined(_WIN32)
    if (configure_terminal_shortcuts() != 0)
    {
        fprintf(stderr, "Warning: Unable to enable Ctrl+X shortcut.\n");
    }
#endif

#ifdef _WIN32
    if (signal(SIGINT, handle_sigint) == SIG_ERR)
    {
        perror("Configure SIGINT handler");
        return exit_program(1);
    }
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) != 0)
    {
        perror("Configure SIGINT handler");
        return exit_program(1);
    }
#endif

    int csv_menu_status = show_csv_control_menu();
    if (exit_requested || interrupt_requested)
    {
        printf("\nHotkey detected. Cleaning up and exiting...\n");
        return exit_program(0);
    }

    if (csv_menu_status < 0)
    {
        return exit_program(1);
    }

    int ensure_status = ensure_csv_exists();
    if (exit_requested || interrupt_requested)
    {
        printf("\nHotkey detected. Cleaning up and exiting...\n");
        return exit_program(0);
    }

    if (ensure_status != 0)
    {
        return exit_program(1);
    }

    if (load_records() != 0)
    {
        printf("Failed to load records.\n");
        return exit_program(1);
    }

    do
    {
        if (exit_requested || interrupt_requested)
        {
            break;
        }

        display_menu();
        choice = read_menu_choice();

        if (exit_requested || interrupt_requested)
        {
            break;
        }

        switch (choice)
        {
        case 1:
            display_records();
            break;
        case 2:
            add_record();
            if (!exit_requested && !interrupt_requested && save_all_records() == 0)
            {
                reload_records_with_warning();
            }
            break;
        case 3:
            search_records();
            break;
        case 4:
            update_record();
            if (!exit_requested && !interrupt_requested && save_all_records() == 0)
            {
                reload_records_with_warning();
            }
            break;
        case 5:
            delete_record();
            if (!exit_requested && !interrupt_requested && save_all_records() == 0)
            {
                reload_records_with_warning();
            }
            break;
        case 6:
            handle_addon_menu();
            break;
        case 7:
            (void)run_unit_test_suite();
            break;
        case 8:
            (void)run_end_to_end_suite();
            break;
        case 9:
            printf("Exiting...\n");
            exit_requested = 1;
            break;
        default:
            printf("Invalid choice. Please try again.\n");
        }

        if (choice >= 1 && choice <= 5 && !exit_requested && !interrupt_requested)
        {
            clear_function_log();
        }
    } while (!exit_requested && choice != 9);

    if (exit_requested && interrupt_requested)
    {
        printf("\nCtrl+C detected. Cleaning up and exiting...\n");
    }

    return exit_program(0);
}
#endif /* UNIT_TEST */

#ifndef UNIT_TEST
void display_menu(void)
{
    printf("\nMachine Maintenance Manager\n");
    printf("1. View Machine Maintenance\n");
    printf("2. Add record\n");
    printf("3. Search record\n");
    printf("4. Update record\n");
    printf("5. Delete record\n");
    printf("6. Add-on tools\n");
    printf("---------\n");
    printf("7. Run unit tests\n");
    printf("8. Run end-to-end tests\n");
    printf("9. Exit\n");
}

static int run_unit_test_suite(void)
{
#if defined(ENABLE_INTERNAL_TESTS)
    printf("\nRunning unit test suite...\n");
    int status = run_internal_unit_tests();
    if (status == 0)
    {
        printf("Unit test suite completed successfully.\n");
    }
    else
    {
        printf("Unit test suite completed with failures.\n");
    }
    fflush(stdout);
    fflush(stderr);
    return status;
#else
    printf("Unit test suite is not available in this build. Rebuild with ENABLE_INTERNAL_TESTS defined.\n");
    return -1;
#endif
}

static int run_end_to_end_suite(void)
{
#if defined(ENABLE_INTERNAL_TESTS)
    printf("\nRunning end-to-end tests...\n");
    int status = run_internal_e2e_tests();
    if (status == 0)
    {
        printf("End-to-end tests completed successfully.\n");
        fprintf(stderr, "End-to-end tests completed successfully.\n");
    }
    else
    {
        printf("End-to-end tests completed with failures.\n");
        fprintf(stderr, "End-to-end tests completed with failures.\n");
    }
    fflush(stdout);
    fflush(stderr);
    return status;
#else
    printf("End-to-end tests are not available in this build. Rebuild with ENABLE_INTERNAL_TESTS defined.\n");
    return -1;
#endif
}
#endif

int ensure_csv_exists(void)
{
    const char *path = maintenance_get_csv_path();
    FILE *f = fopen(path, "r");
    if (f)
    {
        fclose(f);
        return 0;
    }

    printf("No maintenance data file found at '%s'.\n", path);

    FILE *sample = fopen(SAMPLE_CSV_FILE, "r");
    if (!sample)
    {
        printf("Sample file '%s' not found. Creating an empty maintenance file instead.\n", SAMPLE_CSV_FILE);
        if (write_blank_csv(path) != 0)
        {
            return -1;
        }

        printf("Created empty maintenance file at '%s'.\n", path);
        return 0;
    }

    fclose(sample);

    int choice = prompt_copy_from_sample();
    if (choice < 0)
    {
        handle_prompt_result(choice, "Error reading setup choice.");
        return -1;
    }

    if (choice == 1)
    {
        if (copy_example_csv(path) == 0)
        {
            printf("Created '%s' using sample data from '%s'.\n", path, SAMPLE_CSV_FILE);
            return 0;
        }

        printf("Failed to copy sample data. Creating an empty maintenance file instead.\n");
    }

    if (write_blank_csv(path) != 0)
    {
        return -1;
    }

    printf("Created empty maintenance file at '%s'.\n", path);
    return 0;
}

static int prompt_copy_from_sample(void)
{
    char prompt[128];
    snprintf(prompt, sizeof(prompt),
             "Copy sample data from %s? (y/n, Ctrl+X cancel, Ctrl+Z back): ",
             SAMPLE_CSV_FILE);

    int choice = 0;
    int status = prompt_yes_no_common(prompt, "Please enter 'y' or 'n'.", 0, 0, -1,
                                      &choice);
    if (status == INPUT_BACK)
        return INPUT_CANCELLED;
    if (status != INPUT_OK)
        return status;
    return choice;
}

static int write_blank_csv(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        perror("Create CSV");
        return -1;
    }

    if (fputs(CSV_HEADER, f) == EOF)
    {
        perror("Write CSV header");
        fclose(f);
        return -1;
    }

    if (fclose(f) != 0)
    {
        perror("Close CSV");
        return -1;
    }

    return 0;
}

static int copy_example_csv(const char *path)
{
    FILE *src = fopen(SAMPLE_CSV_FILE, "r");
    if (!src)
    {
        perror("Open example CSV");
        return -1;
    }

    FILE *dst = fopen(path, "w");
    if (!dst)
    {
        perror("Create CSV from example");
        fclose(src);
        return -1;
    }

    char buffer[1024];
    size_t bytes_read;
    int error = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        if (fwrite(buffer, 1, bytes_read, dst) != bytes_read)
        {
            perror("Write CSV");
            error = 1;
            break;
        }
    }

    if (!error && ferror(src))
    {
        perror("Read example CSV");
        error = 1;
    }

    if (fclose(dst) != 0)
    {
        perror("Close CSV after copy");
        error = 1;
    }

    if (fclose(src) != 0)
    {
        perror("Close example CSV");
        error = 1;
    }

    return error ? -1 : 0;
}

int load_records(void)
{
    const char *path = maintenance_get_csv_path();
    FILE *file = fopen(path, "r");
    if (!file)
    {
        perror("Open CSV");
        return -1;
    }

    char header[512];
    if (fgets(header, sizeof(header), file) == NULL)
    {
        fclose(file);
        return 0;
    } /* ไม่มีข้อมูล */

    record_count = 0;
    int overflow_detected = 0;
    while (1)
    {
        char n[MAX_NAME], id[MAX_ID], d[MAX_DATE], det[MAX_DETAILS];
        int matched = fscanf(file, " %63[^,],%31[^,],%15[^,],%255[^\n]\n", n, id, d, det);
        if (matched != 4)
            break;

        if (is_record_storage_full())
        {
            overflow_detected = 1;
            continue;
        }

        snprintf(machineName[record_count], MAX_NAME, "%s", n);
        snprintf(machineID[record_count], MAX_ID, "%s", id);
        snprintf(maintenanceDate[record_count], MAX_DATE, "%s", d);
        normalize_imported_details(det);
        snprintf(maintenanceDetails[record_count], MAX_DETAILS, "%s", det);

        record_count++;
    }

    if (overflow_detected)
    {
        printf("Warning: Maximum record capacity of %d reached. Extra records were ignored to prevent overflow.\n",
               MAX_RECORDS);
    }

    if (fclose(file) != 0)
    {
        perror("Close CSV after read");
        return -1;
    }
    return 0;
}

/* Some CSVs may include an extra trailing numeric column (e.g., ",1").
   Since our schema does not allow commas in details, strip a final ",<digits>". */
static void normalize_imported_details(char *details)
{
    if (!details)
    {
        return;
    }

    size_t len = strlen(details);
    if (len == 0)
    {
        return;
    }

    char *last_comma = strrchr(details, ',');
    if (!last_comma)
    {
        return;
    }

    const char *p = last_comma + 1;
    if (*p == '\0')
    {
        return;
    }

    while (*p && isdigit((unsigned char)*p))
    {
        ++p;
    }

    if (*p == '\0' && last_comma > details)
    {
        *last_comma = '\0';
        trim_whitespace(details);
    }
}

int save_all_records(void)
{
    const char *path = maintenance_get_csv_path();
    FILE *file = fopen(path, "w");
    if (!file)
    {
        perror("Open CSV for write");
        return -1;
    }

    fputs(CSV_HEADER, file);
    for (int i = 0; i < record_count; ++i)
    {
        fprintf(file, "%s,%s,%s,%s\n",
                machineName[i], machineID[i], maintenanceDate[i], maintenanceDetails[i]);
    }

    if (fclose(file) != 0)
    {
        perror("Close CSV after write");
        return -1;
    }
    return 0;
}

void display_records(void)
{
    if (record_count == 0)
    {
        printf("No maintenance records.\n");
        return;
    }

    int records_per_page = calculate_records_per_page();
    if (records_per_page <= 0)
    {
        records_per_page = 1;
    }

    int total_pages = (record_count + records_per_page - 1) / records_per_page;
    int current_page = 0;
    char status_message[128];
    status_message[0] = '\0';

    while (!exit_requested && !interrupt_requested)
    {
        if (!exit_requested && !interrupt_requested)
        {
            clear_console_output();
        }

        int start_index = current_page * records_per_page;
        if (start_index >= record_count)
        {
            current_page = (total_pages > 0) ? (total_pages - 1) : 0;
            start_index = current_page * records_per_page;
        }

        int end_index = start_index + records_per_page;
        if (end_index > record_count)
        {
            end_index = record_count;
        }

        printf("\n");
        print_record_table_header();
        for (int i = start_index; i < end_index; ++i)
        {
            print_record_table_row(i);
        }
        print_record_table_footer();

        printf("Records %d-%d of %d | Page %d/%d\n",
               start_index + 1,
               end_index,
               record_count,
               current_page + 1,
               total_pages);

        if (status_message[0] != '\0')
        {
            printf("%s\n", status_message);
            status_message[0] = '\0';
        }

        if (total_pages <= 1)
        {
            break;
        }

        printf("Commands: Enter/right/down arrow=next, left/up arrow=previous, q=quit, number=jump\n");

        char input[32];
        int status = read_paging_command(input, sizeof(input));

        if (status == INPUT_OK)
        {
            int arrow_delta = interpret_paging_arrow(input);
            if (arrow_delta > 0)
            {
                if (current_page + 1 < total_pages)
                {
                    current_page++;
                }
                else
                {
                    snprintf(status_message, sizeof(status_message), "Already on the last page.");
                }
                continue;
            }

            if (arrow_delta < 0)
            {
                if (current_page > 0)
                {
                    current_page--;
                }
                else
                {
                    snprintf(status_message, sizeof(status_message), "Already on the first page.");
                }
                continue;
            }

            if (input[0] == '\0')
            {
                if (current_page + 1 < total_pages)
                {
                    current_page++;
                }
                else
                {
                    snprintf(status_message, sizeof(status_message), "Already on the last page.");
                }
                continue;
            }

            char command = (char)tolower((unsigned char)input[0]);
            if (command == 'q')
            {
                break;
            }
            else if (command == 'n')
            {
                if (current_page + 1 < total_pages)
                {
                    current_page++;
                }
                else
                {
                    snprintf(status_message, sizeof(status_message), "Already on the last page.");
                }
            }
            else if (command == 'p')
            {
                if (current_page > 0)
                {
                    current_page--;
                }
                else
                {
                    snprintf(status_message, sizeof(status_message), "Already on the first page.");
                }
            }
            else
            {
                char *endptr = NULL;
                long page_value = strtol(input, &endptr, 10);
                if (endptr && *endptr == '\0')
                {
                    if (page_value >= 1 && page_value <= total_pages)
                    {
                        current_page = (int)page_value - 1;
                    }
                    else
                    {
                        snprintf(status_message, sizeof(status_message),
                                 "Page must be between 1 and %d.", total_pages);
                    }
                }
                else
                {
                    snprintf(status_message, sizeof(status_message),
                             "Invalid command. Use Enter, n, p, q, or page number.");
                }
            }
        }
        else if (status == INPUT_TOO_LONG)
        {
            snprintf(status_message, sizeof(status_message),
                     "Command too long. Please enter n, p, q, or a page number.");
        }
        else
        {
            if (handle_prompt_result(status, "Error reading pagination command."))
            {
                break;
            }
        }
    }
}

static record_result_t add_record_direct(const char *name, const char *id,
                                         const char *date, const char *details)
{
    if (is_record_storage_full())
    {
        return RECORD_ERROR_STORAGE_FULL;
    }

    if (!name || !id || !date || !details)
    {
        return RECORD_ERROR_INVALID_DATA;
    }

    if (!is_valid_machine_name(name) || !is_valid_machine_id(id) ||
        !is_valid_date(date) || !is_valid_details(details))
    {
        return RECORD_ERROR_INVALID_DATA;
    }

    if (!is_machine_id_unique(id))
    {
        return RECORD_ERROR_DUPLICATE_ID;
    }

    int written_name = snprintf(machineName[record_count], MAX_NAME, "%s", name);
    int written_id = snprintf(machineID[record_count], MAX_ID, "%s", id);
    int written_date = snprintf(maintenanceDate[record_count], MAX_DATE, "%s", date);
    int written_details = snprintf(maintenanceDetails[record_count], MAX_DETAILS, "%s", details);

    if (written_name < 0 || written_id < 0 || written_date < 0 || written_details < 0 ||
        written_name >= MAX_NAME || written_id >= MAX_ID ||
        written_date >= MAX_DATE || written_details >= MAX_DETAILS)
    {
        return RECORD_ERROR_INVALID_DATA;
    }

    record_count++;
    return RECORD_SUCCESS;
}

static record_result_t delete_record_direct(const char *id)
{
    if (!is_valid_machine_id(id))
    {
        return RECORD_ERROR_INVALID_DATA;
    }

    for (int i = 0; i < record_count; ++i)
    {
        if (strcmp(machineID[i], id) == 0)
        {
            for (int j = i; j < record_count - 1; ++j)
            {
                memmove(machineName[j], machineName[j + 1], sizeof(machineName[j]));
                memmove(machineID[j], machineID[j + 1], sizeof(machineID[j]));
                memmove(maintenanceDate[j], maintenanceDate[j + 1], sizeof(maintenanceDate[j]));
                memmove(maintenanceDetails[j], maintenanceDetails[j + 1], sizeof(maintenanceDetails[j]));
            }

            if (record_count > 0)
            {
                int last = record_count - 1;
                memset(machineName[last], 0, sizeof(machineName[last]));
                memset(machineID[last], 0, sizeof(machineID[last]));
                memset(maintenanceDate[last], 0, sizeof(maintenanceDate[last]));
                memset(maintenanceDetails[last], 0, sizeof(maintenanceDetails[last]));
            }

            record_count--;
            return RECORD_SUCCESS;
        }
    }

    return RECORD_ERROR_NOT_FOUND;
}

void add_record(void)
{
    if (is_record_storage_full())
    {
        printf("Storage full.\n");
        return;
    }

    char name[MAX_NAME] = "";
    char id[MAX_ID] = "";
    char date[MAX_DATE] = "";
    char details[MAX_DETAILS] = "";

    const char *error_messages[] = {
        "Error reading machine name.",
        "Error assigning machine ID.",
        "Error reading maintenance date.",
        "Error reading maintenance details."};

    int step = 0;
    while (step < 4)
    {
        int status = INPUT_ERROR;

        switch (step)
        {
        case 0:
            status = prompt_with_validation(name, MAX_NAME, "Enter machine name: ",
                                            is_valid_machine_name,
                                            "Machine name must be printable text without commas or quotes.");
            break;
        case 1:
            status = prompt_machine_id(id, sizeof(id));
            break;
        case 2:
            status = prompt_with_validation(date, MAX_DATE, "Enter maintenance date (YYYY-MM-DD): ",
                                            is_valid_date,
                                            "Date must follow YYYY-MM-DD with a real calendar day.");
            break;
        case 3:
            status = prompt_with_validation(details, MAX_DETAILS, "Enter maintenance details: ",
                                            is_valid_details,
                                            "Details must not be empty, contain commas or quotes.");
            break;
        default:
            status = INPUT_ERROR;
        }

        if (status == INPUT_OK)
        {
            step++;
            continue;
        }

        if (status == INPUT_BACK)
        {
            if (step == 0)
            {
                print_cancel_message();
                return;
            }

            --step;
            continue;
        }

        const int message_count = (int)(sizeof(error_messages) / sizeof(error_messages[0]));
        const char *message = (step >= 0 && step < message_count) ? error_messages[step]
                                                                  : "Unexpected input error.";

        if (handle_prompt_result(status, message))
        {
            return;
        }
    }

    printf("\nNew record preview:\n");
    print_record_preview_from_values(name, id, date, details);

    int confirmed = 0;
    int confirm_status = prompt_confirmation("Do you want to continue? [Y/n] ", &confirmed);
    if (confirm_status != INPUT_OK)
    {
        if (handle_prompt_result(confirm_status, "Error reading confirmation."))
        {
            return;
        }
        return;
    }

    if (!confirmed)
    {
        print_cancel_message();
        return;
    }

    record_result_t insert_status = add_record_direct(name, id, date, details);
    if (insert_status == RECORD_SUCCESS)
    {
        printf("Record added with ID %s.\n", id);
    }
    else
    {
        printf("Failed to add record.\n");
    }
}

void search_records(void)
{
    char q[MAX_NAME];
    int found = 0;

    int status = prompt_with_validation(q, MAX_NAME, "Enter machine name or ID to search: ",
                                        is_non_empty,
                                        "Search text cannot be empty.");
    if (handle_prompt_result(status, "Error reading search query."))
    {
        return;
    }

    int header_printed = 0;
    for (int i = 0; i < record_count; ++i)
    {
        if (string_contains_case_insensitive(machineName[i], q) ||
            string_contains_case_insensitive(machineID[i], q))
        {
            if (!header_printed)
            {
                printf("\nSearch Results:\n");
                print_record_table_header();
                header_printed = 1;
            }
            print_record_table_row(i);
            found++;
        }
    }
    if (!header_printed)
    {
        printf("No records found for '%s'.\n", q);
    }
    else
    {
        print_record_table_footer();
        printf("\nTotal %d record(s).\n", found);
    }
}

void update_record(void)
{
    char id[MAX_ID];

    int status = prompt_with_validation(id, MAX_ID, "Enter machine ID to update: ",
                                        is_valid_machine_id,
                                        "Machine ID may only contain letters, numbers, '-', '_' or '.'.");
    if (handle_prompt_result(status, "Error reading machine ID."))
    {
        return;
    }

    for (int i = 0; i < record_count; ++i)
    {
        if (strcmp(machineID[i], id) == 0)
        {
            char new_name[MAX_NAME];
            char new_date[MAX_DATE];
            char new_details[MAX_DETAILS];

            snprintf(new_name, sizeof(new_name), "%s", machineName[i]);
            snprintf(new_date, sizeof(new_date), "%s", maintenanceDate[i]);
            snprintf(new_details, sizeof(new_details), "%s", maintenanceDetails[i]);

            int step = 0;
            while (step < 3)
            {
                const char *error_message = NULL;
                int prompt_status = INPUT_OK;

                switch (step)
                {
                case 0:
                    printf("Current Name: %s\n", machineName[i]);
                    if (strcmp(new_name, machineName[i]) != 0)
                    {
                        printf("Staged Name: %s\n", new_name);
                    }
                    error_message = "Error updating machine name.";
                    prompt_status = prompt_optional_update("New name (leave blank to keep): ",
                                                           new_name, MAX_NAME,
                                                           is_valid_machine_name,
                                                           "Machine name must be printable text without commas or quotes.");
                    break;
                case 1:
                    printf("Current Date: %s\n", maintenanceDate[i]);
                    if (strcmp(new_date, maintenanceDate[i]) != 0)
                    {
                        printf("Staged Date: %s\n", new_date);
                    }
                    error_message = "Error updating maintenance date.";
                    prompt_status = prompt_optional_update("New date YYYY-MM-DD (leave blank to keep): ",
                                                           new_date, MAX_DATE,
                                                           is_valid_date,
                                                           "Date must follow YYYY-MM-DD with a real calendar day.");
                    break;
                case 2:
                    printf("Current Details: %s\n", maintenanceDetails[i]);
                    if (strcmp(new_details, maintenanceDetails[i]) != 0)
                    {
                        printf("Staged Details: %s\n", new_details);
                    }
                    error_message = "Error updating maintenance details.";
                    prompt_status = prompt_optional_update("New details (leave blank to keep): ",
                                                           new_details, MAX_DETAILS,
                                                           is_valid_details,
                                                           "Details must not be empty, contain commas or quotes.");
                    break;
                default:
                    prompt_status = INPUT_ERROR;
                    error_message = "Unexpected update step.";
                }

                if (prompt_status == INPUT_OK)
                {
                    step++;
                    continue;
                }

                if (prompt_status == INPUT_BACK)
                {
                    if (step == 0)
                    {
                        print_cancel_message();
                        return;
                    }

                    step--;
                    continue;
                }

                if (handle_prompt_result(prompt_status, error_message))
                {
                    return;
                }
            }

            printf("\nCurrent record:\n");
            print_record_table_header();
            print_record_table_row(i);
            print_record_table_footer();

            printf("\nUpdated record preview:\n");
            print_record_preview_from_values(new_name, machineID[i], new_date, new_details);

            int confirmed = 0;
            int confirm_status = prompt_confirmation("Do you want to continue? [Y/n] ", &confirmed);
            if (confirm_status != INPUT_OK)
            {
                if (handle_prompt_result(confirm_status, "Error reading confirmation."))
                {
                    return;
                }
                return;
            }

            if (!confirmed)
            {
                print_cancel_message();
                return;
            }

            snprintf(machineName[i], MAX_NAME, "%s", new_name);
            snprintf(maintenanceDate[i], MAX_DATE, "%s", new_date);
            snprintf(maintenanceDetails[i], MAX_DETAILS, "%s", new_details);

            printf("Record updated.\n");
            return;
        }
    }
    printf("No record with Machine ID '%s'.\n", id);
}

void delete_record(void)
{
    char id[MAX_ID];

    int status = prompt_with_validation(id, MAX_ID, "Enter machine ID to delete: ",
                                        is_valid_machine_id,
                                        "Machine ID may only contain letters, numbers, '-', '_' or '.'.");
    if (handle_prompt_result(status, "Error reading machine ID."))
    {
        return;
    }

    int target_index = -1;
    for (int i = 0; i < record_count; ++i)
    {
        if (strcmp(machineID[i], id) == 0)
        {
            target_index = i;
            break;
        }
    }

    if (target_index < 0)
    {
        printf("No record with Machine ID '%s'.\n", id);
        return;
    }

    printf("\nRecord selected for deletion:\n");
    print_record_table_header();
    print_record_table_row(target_index);
    print_record_table_footer();

    int confirmed = 0;
    int confirm_status = prompt_confirmation("Do you want to continue? [Y/n] ", &confirmed);
    if (confirm_status != INPUT_OK)
    {
        if (handle_prompt_result(confirm_status, "Error reading confirmation."))
        {
            return;
        }
        return;
    }

    if (!confirmed)
    {
        print_cancel_message();
        return;
    }

    record_result_t delete_status = delete_record_direct(id);
    if (delete_status == RECORD_SUCCESS)
    {
        printf("Record deleted.\n");
        return;
    }

    if (delete_status == RECORD_ERROR_NOT_FOUND)
    {
        printf("No record with Machine ID '%s'.\n", id);
        return;
    }

    printf("Unable to delete record.\n");
}

#if defined(UNIT_TEST)

typedef struct test_stats
{
    int total;
    int failed;
} test_stats_t;

static void test_assert(test_stats_t *stats, int condition, const char *message)
{
    if (!stats || !message)
    {
        return;
    }

    stats->total++;
    if (condition)
    {
        printf("   [PASS] %s\n", message);
    }
    else
    {
        stats->failed++;
        printf("   [FAIL] %s\n", message);
    }
}

static void print_test_section_header(const char *title)
{
    if (!title)
    {
        title = "";
    }

    printf("\n=== %s ===\n", title);
}

static int derive_scratch_csv_path(char *buffer, size_t size)
{
    if (!buffer || size == 0)
    {
        return -1;
    }

#if defined(_WIN32)
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || tmpdir[0] == '\0')
    {
        tmpdir = getenv("TEMP");
    }
    if (!tmpdir || tmpdir[0] == '\0')
    {
        tmpdir = getenv("TMP");
    }
    if (!tmpdir || tmpdir[0] == '\0')
    {
        tmpdir = ".";
    }

    size_t dir_len = strlen(tmpdir);
    int needs_sep = 1;
    if (dir_len > 0)
    {
        char last = tmpdir[dir_len - 1];
        if (last == '/' || last == '\\')
        {
            needs_sep = 0;
        }
    }

    char template_buffer[CSV_PATH_MAX];
    int written = 0;
    if (needs_sep)
    {
        written = snprintf(template_buffer, sizeof(template_buffer), "%s%cmaintenance_e2e_XXXXXX.csv", tmpdir, '\\');
    }
    else
    {
        written = snprintf(template_buffer, sizeof(template_buffer), "%smaintenance_e2e_XXXXXX.csv", tmpdir);
    }

    if (written < 0 || (size_t)written >= sizeof(template_buffer))
    {
        return -1;
    }

    if (_mktemp_s(template_buffer, strlen(template_buffer) + 1) != 0)
    {
        return -1;
    }

    int final_written = snprintf(buffer, size, "%s", template_buffer);
    if (final_written < 0 || (size_t)final_written >= size)
    {
        return -1;
    }
    return 0;
#else
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || tmpdir[0] == '\0')
    {
        tmpdir = "/tmp";
    }

    size_t dir_len = strlen(tmpdir);
    int needs_sep = 1;
    if (dir_len > 0 && (tmpdir[dir_len - 1] == '/' || tmpdir[dir_len - 1] == '\\'))
    {
        needs_sep = 0;
    }

    char template_buffer[CSV_PATH_MAX];
    int written = 0;
    if (needs_sep)
    {
        written = snprintf(template_buffer, sizeof(template_buffer), "%s/maintenance_e2e_XXXXXX", tmpdir);
    }
    else
    {
        written = snprintf(template_buffer, sizeof(template_buffer), "%smaintenance_e2e_XXXXXX", tmpdir);
    }

    if (written < 0 || (size_t)written >= sizeof(template_buffer))
    {
        return -1;
    }

    int fd = mkstemp(template_buffer);
    if (fd < 0)
    {
        return -1;
    }
    close(fd);

    int final_written = snprintf(buffer, size, "%s.csv", template_buffer);
    if (final_written < 0 || (size_t)final_written >= size)
    {
        remove(template_buffer);
        return -1;
    }

    if (rename(template_buffer, buffer) != 0)
    {
        remove(template_buffer);
        return -1;
    }
    return 0;
#endif
}

void run_end_to_end_test(test_stats_t *stats)
{
    print_test_section_header("End-to-End Test: workflow");

    char original_path[CSV_PATH_MAX];
    snprintf(original_path, sizeof(original_path), "%s", maintenance_get_csv_path());

    char scratch_path[CSV_PATH_MAX] = {0};
    if (derive_scratch_csv_path(scratch_path, sizeof(scratch_path)) != 0)
    {
        test_assert(stats, 0, "Generate scratch CSV path");
        return;
    }

    FILE *scratch = fopen(scratch_path, "w+");
    test_assert(stats, scratch != NULL, "Open scratch CSV path");
    if (!scratch)
    {
        remove(scratch_path);
        maintenance_set_csv_path(original_path);
        return;
    }
    fclose(scratch);

    int path_switched = 0;
    if (maintenance_set_csv_path(scratch_path) != 0)
    {
        test_assert(stats, 0, "Switch to test CSV path");
        goto cleanup;
    }
    path_switched = 1;

    clear_record_storage();

    test_assert(stats, add_record_direct("Hydraulic Press", "101", "2025-08-01",
                                         "Changed hydraulic fluid") == RECORD_SUCCESS,
                "Add first record");
    test_assert(stats, add_record_direct("Laser Cutter", "202", "2025-08-05",
                                         "Replaced air filter") == RECORD_SUCCESS,
                "Add second record");

    test_assert(stats, save_all_records() == 0, "Persist records to CSV");

    clear_record_storage();
    test_assert(stats, load_records() == 0, "Reload records from CSV");
    test_assert(stats, record_count == 2, "Record count after reload is 2");
    test_assert(stats, strcmp(machineName[0], "Hydraulic Press") == 0, "First record name matches");
    test_assert(stats, strcmp(machineID[1], "202") == 0, "Second record ID matches");

    test_assert(stats, delete_record_direct("101") == RECORD_SUCCESS, "Delete first record");
    test_assert(stats, record_count == 1, "Record count after delete is 1");
    test_assert(stats, strcmp(machineID[0], "202") == 0, "Remaining record has expected ID");

    test_assert(stats, save_all_records() == 0, "Persist updated records");

    clear_record_storage();
    test_assert(stats, load_records() == 0, "Reload updated records");
    test_assert(stats, record_count == 1, "Final record count is 1");
    test_assert(stats, strcmp(machineName[0], "Laser Cutter") == 0, "Final record still correct");

cleanup:
    if (path_switched)
    {
        clear_record_storage();
        int restore_status = maintenance_set_csv_path(original_path);
        test_assert(stats, restore_status == 0, "Restore original CSV path");
        if (restore_status == 0)
        {
            test_assert(stats, load_records() == 0, "Reload original data");
        }
        else
        {
            test_assert(stats, 0, "Reload original data");
        }
    }
    else
    {
        maintenance_set_csv_path(original_path);
    }

    if (scratch_path[0] != '\0')
    {
        remove(scratch_path);
    }
    clear_record_storage();
}

#endif /* UNIT_TEST */


int safe_input(char *buffer, int size, const char *prompt)
{
    if (size <= 0)
    {
        return INPUT_ERROR;
    }

    if (exit_requested || interrupt_requested)
    {
        if (buffer && size > 0)
        {
            buffer[0] = '\0';
        }
        return INPUT_EXIT;
    }

    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buffer, size, stdin) == NULL)
    {
        if (exit_requested || interrupt_requested)
        {
            buffer[0] = '\0';
            return INPUT_EXIT;
        }

        return INPUT_ERROR;
    }

    int truncated = (strchr(buffer, '\n') == NULL);
    if (contains_cancel_signal(buffer))
    {
        if (truncated)
        {
            flush_line();
        }
        buffer[0] = '\0';
        return INPUT_CANCELLED;
    }

    if (contains_back_signal(buffer))
    {
        if (truncated)
        {
            flush_line();
        }
        buffer[0] = '\0';
        return INPUT_BACK;
    }

    sanitize_input(buffer);

    if (truncated)
    {
        printf("Input too long. Maximum length is %d characters.\n", size - 1);
        buffer[0] = '\0';
        return INPUT_TOO_LONG;
    }

    return INPUT_OK;
}

int prompt_with_validation(char *buffer, int size, const char *prompt,
                           int (*validator)(const char *), const char *error_message)
{
    if (size <= 0)
    {
        return INPUT_ERROR;
    }

    while (1)
    {
        int status = safe_input(buffer, size, prompt);
        if (status == INPUT_EXIT)
        {
            return INPUT_EXIT;
        }
        if (status == INPUT_BACK)
        {
            return INPUT_BACK;
        }
        if (status == INPUT_CANCELLED)
        {
            return INPUT_CANCELLED;
        }

        if (status == INPUT_ERROR)
        {
            return INPUT_ERROR;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (!validator || validator(buffer))
        {
            return INPUT_OK;
        }

        if (error_message)
        {
            printf("%s\n", error_message);
        }
    }
}

#ifndef UNIT_TEST
int read_menu_choice(void)
{
    char buffer[16];

    while (1)
    {
        int status = safe_input(buffer, sizeof(buffer), "Enter your choice : ");
        if (status == INPUT_EXIT)
        {
            return 9;
        }
        if (status == INPUT_BACK || status == INPUT_CANCELLED)
        {
            clear_console_output();
            display_menu();
            continue;
        }

        if (status == INPUT_ERROR)
        {
            printf("Input error detected. Exiting menu.\n");
            return 9;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (buffer[0] == '\0')
        {
            clear_console_output();
            printf("Invalid choice. Please enter a number between 1 and 9.\n");
            display_menu();
            continue;
        }

        char *endptr = NULL;
        long value = strtol(buffer, &endptr, 10);
        if (endptr != NULL && *endptr == '\0' && value >= 1 && value <= 9)
        {
            return (int)value;
        }

        clear_console_output();
        printf("Invalid choice. Please enter a number between 1 and 9.\n");
        display_menu();
    }
}
#endif

void trim_whitespace(char *str)
{
    if (!str)
    {
        return;
    }

    char *start = str;
    while (*start && isspace((unsigned char)*start))
    {
        start++;
    }

    if (start != str)
    {
        memmove(str, start, strlen(start) + 1);
    }

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1]))
    {
        str[len - 1] = '\0';
        len--;
    }
}

static void flush_line(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
    {
    }
}

static void UNUSED_FN clear_record_storage(void)
{
    memset(machineName, 0, sizeof(machineName));
    memset(machineID, 0, sizeof(machineID));
    memset(maintenanceDate, 0, sizeof(maintenanceDate));
    memset(maintenanceDetails, 0, sizeof(maintenanceDetails));
    record_count = 0;
}

#ifndef UNIT_TEST
static void UNUSED_FN handle_sigint(int signal)
{
    (void)signal;
    request_exit();
}
#endif

void sanitize_input(char *buffer)
{
    if (!buffer)
    {
        return;
    }

    size_t len = strcspn(buffer, "\r\n");
    if (buffer[len] != '\0')
    {
        buffer[len] = '\0';
    }
    else
    {
        flush_line();
    }

    trim_whitespace(buffer);
}

int is_non_empty(const char *str)
{
    return (str != NULL && str[0] != '\0');
}

int contains_disallowed_csv_chars(const char *str)
{
    if (!str)
    {
        return 0;
    }

    for (const char *p = str; *p; ++p)
    {
        if (*p == ',' || *p == '"')
        {
            return 1;
        }
    }
    return 0;
}

int contains_cancel_signal(const char *str)
{
    if (!str)
    {
        return 0;
    }

    for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; ++p)
    {
        if (*p == 0x18)
        {
            return 1;
        }
    }

    return 0;
}

static int contains_back_signal(const char *str)
{
    if (!str)
    {
        return 0;
    }

    for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; ++p)
    {
        if (*p == 0x1A)
        {
            return 1;
        }
    }

    return 0;
}

int is_valid_machine_name(const char *str)
{
    if (!is_non_empty(str) || contains_disallowed_csv_chars(str))
    {
        return 0;
    }

    for (const char *p = str; *p; ++p)
    {
        if (!isprint((unsigned char)*p))
        {
            return 0;
        }
    }
    return 1;
}

int is_valid_machine_id(const char *str)
{
    if (!str || str[0] == '\0')
    {
        return 0;
    }

    for (const char *p = str; *p; ++p)
    {
        unsigned char ch = (unsigned char)*p;
        if (!isalnum(ch) && ch != '-' && ch != '_' && ch != '.')
        {
            return 0;
        }
    }

    return 1;
}

int is_valid_date(const char *str)
{
    if (!str)
    {
        return 0;
    }

    if (strlen(str) != 10)
    {
        return 0;
    }

    for (int i = 0; i < 10; ++i)
    {
        if (i == 4 || i == 7)
        {
            if (str[i] != '-')
            {
                return 0;
            }
        }
        else if (!isdigit((unsigned char)str[i]))
        {
            return 0;
        }
    }

    int year = (str[0] - '0') * 1000 + (str[1] - '0') * 100 + (str[2] - '0') * 10 + (str[3] - '0');
    int month = (str[5] - '0') * 10 + (str[6] - '0');
    int day = (str[8] - '0') * 10 + (str[9] - '0');

    if (month < 1 || month > 12)
    {
        return 0;
    }

    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (is_leap)
    {
        days_in_month[1] = 29;
    }

    if (day < 1 || day > days_in_month[month - 1])
    {
        return 0;
    }

    return 1;
}

int is_valid_details(const char *str)
{
    if (!is_non_empty(str) || contains_disallowed_csv_chars(str))
    {
        return 0;
    }

    for (const char *p = str; *p; ++p)
    {
        if (!isprint((unsigned char)*p))
        {
            return 0;
        }
    }
    return 1;
}

int is_record_storage_full(void)
{
    return (record_count < 0) || (record_count >= MAX_RECORDS);
}

int prompt_optional_update(const char *prompt, char *dest, int dest_size,
                           int (*validator)(const char *), const char *error_message)
{
    if (!dest || dest_size <= 0)
    {
        return INPUT_ERROR;
    }

    char buffer[dest_size];

    while (1)
    {
        int status = safe_input(buffer, sizeof(buffer), prompt);
        if (status == INPUT_EXIT)
        {
            return INPUT_EXIT;
        }
        if (status == INPUT_BACK)
        {
            return INPUT_BACK;
        }
        if (status == INPUT_CANCELLED)
        {
            return INPUT_CANCELLED;
        }

        if (status == INPUT_ERROR)
        {
            printf("Input error. Field unchanged.\n");
            return INPUT_ERROR;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (buffer[0] == '\0')
        {
            return INPUT_OK;
        }

        if (!validator || validator(buffer))
        {
            snprintf(dest, dest_size, "%s", buffer);
            return INPUT_OK;
        }

        if (error_message)
        {
            printf("%s\n", error_message);
        }
    }
}

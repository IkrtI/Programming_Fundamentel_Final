#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "maintenance.h"

static char csv_file_path[CSV_PATH_MAX] = CSV_FILE_DEFAULT;
static const char *SAMPLE_CSV_FILE = "maintenance-example.csv";
static const char CSV_HEADER[] = "MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n";

char machineName[MAX_RECORDS][MAX_NAME];
char machineID[MAX_RECORDS][MAX_ID];
char maintenanceDate[MAX_RECORDS][MAX_DATE];
char maintenanceDetails[MAX_RECORDS][MAX_DETAILS];
int record_count = 0;

static volatile sig_atomic_t interrupt_requested = 0;

static void print_cancel_message(void)
{
    printf("\nOperation cancelled. Returning to Machine Maintenance Manager.\n");
}

static void clear_console(void);
static void clear_record_storage(void);
static void handle_sigint(int signal);
static void flush_line(void);
static int prompt_copy_from_sample(void);
static int write_blank_csv(const char *path);
static int copy_example_csv(const char *path);

static int handle_prompt_result(int status, const char *error_message)
{
    if (status == INPUT_CANCELLED)
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

#ifndef UNIT_TEST
void display_menu(void);
int read_menu_choice(void);
void run_end_to_end_tests(void);

int main(void)
{
    int choice;

    signal(SIGINT, handle_sigint);

    if (ensure_csv_exists() != 0)
        return 1;

    // Load records once at startup
    if (load_records() != 0)
    {
        printf("Failed to load records.\n");
        return 1;
    }

    do
    {
        if (interrupt_requested)
        {
            clear_record_storage();
            printf("\nCtrl+C detected. Cleared maintenance data from memory before exiting.\n");
            break;
        }

        display_menu();
        choice = read_menu_choice();

        if (interrupt_requested)
        {
            clear_record_storage();
            printf("\nCtrl+C detected. Cleared maintenance data from memory before exiting.\n");
            break;
        }

        switch (choice)
        {
        case 1:
            display_records();
            break;
        case 2:
            add_record();
            if (save_all_records() == 0)
            {
                reload_records_with_warning();
            }
            break;
        case 3:
            search_records();
            break;
        case 4:
            update_record();
            if (save_all_records() == 0)
            {
                reload_records_with_warning();
            }
            break;
        case 5:
            delete_record();
            if (save_all_records() == 0)
            {
                reload_records_with_warning();
            }
            break;
        case 6:
            clear_console();
            break;
        case 7:
            clear_record_storage();
            printf("Exiting...\n");
            break;
        case 8:
            run_end_to_end_tests();
            break;
        default:
            printf("Invalid choice. Please try again.\n");
        }
    } while (choice != 7);

    return 0;
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
    printf("6. Clear console\n");
    printf("7. Exit\n");
    printf("8. Run end-to-end tests\n");
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
    snprintf(prompt, sizeof(prompt), "Copy sample data from %s? (y/n): ", SAMPLE_CSV_FILE);

    while (1)
    {
        char response[8];
        int status = safe_input(response, sizeof(response), prompt);

        if (status == INPUT_CANCELLED)
        {
            return INPUT_CANCELLED;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (status == INPUT_ERROR)
        {
            return INPUT_ERROR;
        }

        if (status != INPUT_OK)
        {
            continue;
        }

        if (response[0] == '\0')
        {
            printf("Please enter 'y' or 'n'.\n");
            continue;
        }

        char c = (char)tolower((unsigned char)response[0]);
        if (c == 'y')
        {
            return 1;
        }
        if (c == 'n')
        {
            return 0;
        }

        printf("Please enter 'y' or 'n'.\n");
    }
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

    printf("\n%-20s %-12s %-12s %-30s\n", "Machine Name", "Machine ID", "Date", "Details");
    printf("-------------------------------------------------------------------------------\n");
    for (int i = 0; i < record_count; ++i)
    {
        printf("%-20s %-12s %-12s %-30s\n",
               machineName[i], machineID[i], maintenanceDate[i], maintenanceDetails[i]);
    }
}

void add_record(void)
{
    if (is_record_storage_full())
    {
        printf("Storage full.\n");
        return;
    }

    char n[MAX_NAME], id[MAX_ID], d[MAX_DATE], det[MAX_DETAILS];

    int status = prompt_with_validation(n, MAX_NAME, "Enter machine name: ",
                                        is_valid_machine_name,
                                        "Machine name must be printable text without commas or quotes.");
    if (handle_prompt_result(status, "Error reading machine name."))
    {
        return;
    }

    status = prompt_with_validation(id, MAX_ID, "Enter machine ID: ",
                                    is_valid_machine_id,
                                    "Machine ID must use letters, numbers, dashes, underscores or dots only.");
    if (handle_prompt_result(status, "Error reading machine ID."))
    {
        return;
    }

    for (int i = 0; i < record_count; ++i)
    {
        if (strcmp(machineID[i], id) == 0)
        {
            printf("Machine ID '%s' already exists.\n", id);
            return;
        }
    }

    status = prompt_with_validation(d, MAX_DATE, "Enter maintenance date (YYYY-MM-DD): ",
                                    is_valid_date,
                                    "Date must follow YYYY-MM-DD with a real calendar day.");
    if (handle_prompt_result(status, "Error reading maintenance date."))
    {
        return;
    }

    status = prompt_with_validation(det, MAX_DETAILS, "Enter maintenance details: ",
                                    is_valid_details,
                                    "Details must not be empty, contain commas or quotes.");
    if (handle_prompt_result(status, "Error reading maintenance details."))
    {
        return;
    }

    snprintf(machineName[record_count], MAX_NAME, "%s", n);
    snprintf(machineID[record_count], MAX_ID, "%s", id);
    snprintf(maintenanceDate[record_count], MAX_DATE, "%s", d);
    snprintf(maintenanceDetails[record_count], MAX_DETAILS, "%s", det);

    record_count++;
    printf("Record added.\n");
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

    printf("\nSearch Results:\n");
    printf("%-20s %-12s %-12s %-30s\n", "Machine Name", "Machine ID", "Date", "Details");
    printf("-------------------------------------------------------------------------------\n");

    for (int i = 0; i < record_count; ++i)
    {
        if (strstr(machineName[i], q) != NULL || strstr(machineID[i], q) != NULL)
        {
            printf("%-20s %-12s %-12s %-30s\n",
                   machineName[i], machineID[i], maintenanceDate[i], maintenanceDetails[i]);
            found++;
        }
    }
    if (found == 0)
        printf("No records found for '%s'.\n", q);
    else
        printf("\nTotal %d record(s).\n", found);
}

void update_record(void)
{
    char id[MAX_ID];

    int status = prompt_with_validation(id, MAX_ID, "Enter machine ID to update: ",
                                        is_valid_machine_id,
                                        "Machine ID must use letters, numbers, dashes, underscores or dots only.");
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

            printf("Current Name: %s\n", machineName[i]);
            status = prompt_optional_update("New name (leave blank to keep): ",
                                             new_name, MAX_NAME,
                                             is_valid_machine_name,
                                             "Machine name must be printable text without commas or quotes.");
            if (status != INPUT_OK && handle_prompt_result(status, "Error updating machine name."))
            {
                return;
            }

            printf("Current Date: %s\n", maintenanceDate[i]);
            status = prompt_optional_update("New date YYYY-MM-DD (leave blank to keep): ",
                                             new_date, MAX_DATE,
                                             is_valid_date,
                                             "Date must follow YYYY-MM-DD with a real calendar day.");
            if (status != INPUT_OK && handle_prompt_result(status, "Error updating maintenance date."))
            {
                return;
            }

            printf("Current Details: %s\n", maintenanceDetails[i]);
            status = prompt_optional_update("New details (leave blank to keep): ",
                                             new_details, MAX_DETAILS,
                                             is_valid_details,
                                             "Details must not be empty, contain commas or quotes.");
            if (status != INPUT_OK && handle_prompt_result(status, "Error updating maintenance details."))
            {
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
                                        "Machine ID must use letters, numbers, dashes, underscores or dots only.");
    if (handle_prompt_result(status, "Error reading machine ID."))
    {
        return;
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
            record_count--;
            printf("Record deleted.\n");
            return;
        }
    }
    printf("No record with Machine ID '%s'.\n", id);
}

int safe_input(char *buffer, int size, const char *prompt)
{
    if (size <= 0)
    {
        return INPUT_ERROR;
    }

    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buffer, size, stdin) == NULL)
    {
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
        int status = safe_input(buffer, sizeof(buffer), "Enter your choice: ");
        if (status == INPUT_CANCELLED)
        {
            print_cancel_message();
            continue;
        }

        if (status == INPUT_ERROR)
        {
            printf("Input error detected. Exiting menu.\n");
            return 7;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (buffer[0] == '\0')
        {
            printf("Invalid choice. Please enter a number between 1 and 8.\n");
            continue;
        }

        char *endptr = NULL;
        long value = strtol(buffer, &endptr, 10);
        if (endptr != NULL && *endptr == '\0' && value >= 1 && value <= 8)
        {
            return (int)value;
        }

        printf("Invalid choice. Please enter a number between 1 and 8.\n");
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

static void clear_record_storage(void)
{
    memset(machineName, 0, sizeof(machineName));
    memset(machineID, 0, sizeof(machineID));
    memset(maintenanceDate, 0, sizeof(maintenanceDate));
    memset(maintenanceDetails, 0, sizeof(maintenanceDetails));
    record_count = 0;
}

static void handle_sigint(int signal)
{
    (void)signal;
    interrupt_requested = 1;
}

static void clear_console(void)
{
#ifdef _WIN32
    int rc = system("cls");
    if (rc == -1)
    {
        perror("system");
    }
#else
    int rc = system("clear");
    if (rc == -1)
    {
        perror("system");
    }
#endif
}

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
    if (!is_non_empty(str) || contains_disallowed_csv_chars(str))
    {
        return 0;
    }

    for (const char *p = str; *p; ++p)
    {
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == '.'))
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

#ifndef UNIT_TEST

/* =============================== */
/* End-to-End Test Harness Helpers */
/* =============================== */

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
        fputs("MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n", file);
        fputs("Hydraulic Press,HP-001,2025-08-01,Changed hydraulic fluid\n", file);
        fputs("CNC Mill,CNC-12,2025-08-03,Recalibrated spindle\n", file);
        fputs("3D Printer,PR-22,2025-08-04,Updated firmware\n", file);
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
    printf("📋 Scenario: Add → Save → Update → Delete → Reload\n\n");

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

    printf("   Step 5: Delete record via delete_record()\n");
    {
        char input[] = "LC-07\n";
        int saved_fd = -1;
        FILE *temp = NULL;
        ASSERT_EQ_INT(0, redirect_stdin_from_string(input, &saved_fd, &temp), "Simulate delete input");
        delete_record();
        restore_stdin(saved_fd, temp);
    }

    ASSERT_EQ_INT(0, record_count, "Record count is zero after delete");
    ASSERT_EQ_INT(0, save_all_records(), "Saved file after delete");

    printf("   Step 6: Reload to confirm data removal\n");
    reset_in_memory_records();
    ASSERT_EQ_INT(0, load_records(), "Load file after deletion");
    ASSERT_EQ_INT(0, record_count, "Record count remains zero after reload");

    int csv_lines = count_csv_data_lines(test_path);
    ASSERT_EQ_INT(0, csv_lines, "CSV file has no data rows after deletion");

    remove_file_if_exists(test_path);
    reset_in_memory_records();
    maintenance_set_csv_path(original_path);

    printf("\n✅ END-TO-END TEST CASE 3: COMPLETED\n");
}

void run_end_to_end_tests(void)
{
    assertions_run = 0;
    assertions_failed = 0;

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
    }
    else
    {
        printf("⚠️ Result: End-to-End tests encountered failures\n");
    }
}

#endif /* UNIT_TEST */

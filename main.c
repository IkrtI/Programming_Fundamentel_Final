#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "maintenance.h"

static char csv_file_path[CSV_PATH_MAX] = CSV_FILE_DEFAULT;
static const char *SAMPLE_CSV_FILE = "maintenance-example.csv";
static const char CSV_HEADER[] = "MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n";

char machineName[MAX_RECORDS][MAX_NAME];
char machineID[MAX_RECORDS][MAX_ID];
char maintenanceDate[MAX_RECORDS][MAX_DATE];
char maintenanceDetails[MAX_RECORDS][MAX_DETAILS];
int record_count = 0;

static void print_cancel_message(void)
{
    printf("\nOperation cancelled. Returning to Machine Maintenance Manager.\n");
}

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

int main(void)
{
    int choice;

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
        display_menu();
        choice = read_menu_choice();

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
            printf("Exiting...\n");
            break;
        default:
            printf("Invalid choice. Please try again.\n");
        }
    } while (choice != 6);

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
    printf("6. Exit\n");
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
            return 6;
        }

        if (status == INPUT_TOO_LONG)
        {
            continue;
        }

        if (buffer[0] == '\0')
        {
            printf("Invalid choice. Please enter a number between 1 and 6.\n");
            continue;
        }

        char *endptr = NULL;
        long value = strtol(buffer, &endptr, 10);
        if (endptr != NULL && *endptr == '\0' && value >= 1 && value <= 6)
        {
            return (int)value;
        }

        printf("Invalid choice. Please enter a number between 1 and 6.\n");
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

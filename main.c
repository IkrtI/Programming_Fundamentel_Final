#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "maintenance.h"

static char csv_file_path[CSV_PATH_MAX] = CSV_FILE_DEFAULT;

char machineName[MAX_RECORDS][MAX_NAME];
char machineID[MAX_RECORDS][MAX_ID];
char maintenanceDate[MAX_RECORDS][MAX_DATE];
char maintenanceDetails[MAX_RECORDS][MAX_DETAILS];
int record_count = 0;

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

    if (snprintf(csv_file_path, sizeof(csv_file_path), "%s", path) < 0)
    {
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
                // Reload to reflect changes
                load_records();
            }
            break;
        case 3:
            search_records();
            break;
        case 4:
            update_record();
            if (save_all_records() == 0)
            {
                // Reload to reflect changes
                load_records();  
            }
            break;
        case 5:
            delete_record();
            if (save_all_records() == 0)
            {
                // Reload to reflect changes
                load_records();
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

    f = fopen(path, "w");
    if (!f)
    {
        perror("Create CSV");
        return -1;
    }
    fprintf(f, "MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n");
    if (fclose(f) != 0)
    {
        perror("Close CSV");
        return -1;
    }
    return 0;
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
    while (record_count < MAX_RECORDS)
    {
        char n[MAX_NAME], id[MAX_ID], d[MAX_DATE], det[MAX_DETAILS];
        int matched = fscanf(file, " %63[^,],%31[^,],%15[^,],%255[^\n]\n", n, id, d, det);
        if (matched != 4)
            break;

        snprintf(machineName[record_count], MAX_NAME, "%s", n);
        snprintf(machineID[record_count], MAX_ID, "%s", id);
        snprintf(maintenanceDate[record_count], MAX_DATE, "%s", d);
        snprintf(maintenanceDetails[record_count], MAX_DETAILS, "%s", det);

        record_count++;
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

    fprintf(file, "MachineName,MachineID,MaintenanceDate,MaintenanceDetails\n");
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
    if (record_count >= MAX_RECORDS)
    {
        printf("Storage full.\n");
        return;
    }

    char n[MAX_NAME], id[MAX_ID], d[MAX_DATE], det[MAX_DETAILS];

    if (prompt_with_validation(n, MAX_NAME, "Enter machine name: ",
                               is_valid_machine_name,
                               "Machine name must be printable text without commas or quotes.") != 0)
    {
        printf("Error reading machine name.\n");
        return;
    }

    if (prompt_with_validation(id, MAX_ID, "Enter machine ID: ",
                               is_valid_machine_id,
                               "Machine ID must use letters, numbers, dashes, underscores or dots only.") != 0)
    {
        printf("Error reading machine ID.\n");
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

    if (prompt_with_validation(d, MAX_DATE, "Enter maintenance date (YYYY-MM-DD): ",
                               is_valid_date,
                               "Date must follow YYYY-MM-DD with a real calendar day.") != 0)
    {
        printf("Error reading maintenance date.\n");
        return;
    }

    if (prompt_with_validation(det, MAX_DETAILS, "Enter maintenance details: ",
                               is_valid_details,
                               "Details must not be empty, contain commas or quotes.") != 0)
    {
        printf("Error reading maintenance details.\n");
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

    if (prompt_with_validation(q, MAX_NAME, "Enter machine name or ID to search: ",
                               is_non_empty,
                               "Search text cannot be empty.") != 0)
    {
        printf("Error reading search query.\n");
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

    if (prompt_with_validation(id, MAX_ID, "Enter machine ID to update: ",
                               is_valid_machine_id,
                               "Machine ID must use letters, numbers, dashes, underscores or dots only.") != 0)
    {
        printf("Error reading machine ID.\n");
        return;
    }

    for (int i = 0; i < record_count; ++i)
    {
        if (strcmp(machineID[i], id) == 0)
        {
            printf("Current Name: %s\n", machineName[i]);
            prompt_optional_update("New name (leave blank to keep): ",
                                   machineName[i], MAX_NAME,
                                   is_valid_machine_name,
                                   "Machine name must be printable text without commas or quotes.");

            printf("Current Date: %s\n", maintenanceDate[i]);
            prompt_optional_update("New date YYYY-MM-DD (leave blank to keep): ",
                                   maintenanceDate[i], MAX_DATE,
                                   is_valid_date,
                                   "Date must follow YYYY-MM-DD with a real calendar day.");

            printf("Current Details: %s\n", maintenanceDetails[i]);
            prompt_optional_update("New details (leave blank to keep): ",
                                   maintenanceDetails[i], MAX_DETAILS,
                                   is_valid_details,
                                   "Details must not be empty, contain commas or quotes.");

            printf("Record updated.\n");
            return;
        }
    }
    printf("No record with Machine ID '%s'.\n", id);
}

void delete_record(void)
{
    char id[MAX_ID];
    
    if (prompt_with_validation(id, MAX_ID, "Enter machine ID to delete: ",
                               is_valid_machine_id,
                               "Machine ID must use letters, numbers, dashes, underscores or dots only.") != 0)
    {
        printf("Error reading machine ID.\n");
        return;
    }

    for (int i = 0; i < record_count; ++i)
    {
        if (strcmp(machineID[i], id) == 0)
        {
            for (int j = i; j < record_count - 1; ++j)
            {

                snprintf(machineName[j], MAX_NAME, "%s", machineName[j + 1]);
                snprintf(machineID[j], MAX_ID, "%s", machineID[j + 1]);
                snprintf(maintenanceDate[j], MAX_DATE, "%s", maintenanceDate[j + 1]);
                snprintf(maintenanceDetails[j], MAX_DETAILS, "%s", maintenanceDetails[j + 1]);
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
        return -1;
    }

    printf("%s", prompt);
    if (fgets(buffer, size, stdin) == NULL)
    {
        return -1;
    }

    sanitize_input(buffer);
    return 0;
}

int prompt_with_validation(char *buffer, int size, const char *prompt,
                           int (*validator)(const char *), const char *error_message)
{
    if (size <= 0)
    {
        return -1;
    }

    while (1)
    {
        if (safe_input(buffer, size, prompt) != 0)
        {
            return -1;
        }

        if (!validator || validator(buffer))
        {
            return 0;
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
        if (safe_input(buffer, sizeof(buffer), "Enter your choice: ") != 0)
        {
            printf("Input error detected. Exiting menu.\n");
            return 6;
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

void flush_line(void)
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

void prompt_optional_update(const char *prompt, char *dest, int dest_size,
                            int (*validator)(const char *), const char *error_message)
{
    if (!dest || dest_size <= 0)
    {
        return;
    }

    char buffer[MAX_DETAILS];

    while (1)
    {
        printf("%s", prompt);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            printf("Input error. Field unchanged.\n");
            return;
        }

        sanitize_input(buffer);

        if (buffer[0] == '\0')
        {
            return;
        }

        if (!validator || validator(buffer))
        {
            snprintf(dest, dest_size, "%s", buffer);
            return;
        }

        if (error_message)
        {
            printf("%s\n", error_message);
        }
    }
}

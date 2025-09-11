#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CSV_FILE "maintenance.csv"

#define MAX_NAME 64
#define MAX_ID 32
#define MAX_DATE 16
#define MAX_DETAILS 256
#define LINE_BUF 1024

typedef struct
{
    char machineName[MAX_NAME];
    char machineID[MAX_ID];
    char maintenanceDate[MAX_DATE];
    char maintenanceDetails[MAX_DETAILS];
} MaintenanceRecord;

int display_menu();
int load_records(MaintenanceRecord **records, int *count);
int display_records(MaintenanceRecord *records, int count);
int add_record(MaintenanceRecord **records, int *count);
int search_records(MaintenanceRecord *records, int count);
int update_record(MaintenanceRecord *records, int count);
int delete_record(MaintenanceRecord **records, int *count);
static void read_line_prompt(const char *prompt, char *buffer, size_t size);
static void trim_newline(char *s);
static void strip(char *s);

int main()
{
    MaintenanceRecord *records = NULL;
    int record_count = 0;
    char choice[8];

    
    int num_choice = 0;
    do
    {
        display_menu();
        read_line_prompt("Enter your choice: ", choice, sizeof(choice));
        num_choice = atoi(choice);
        load_records(&records, &record_count);
        switch (num_choice)
        {
        case 1:
            display_records(records, record_count);
            break;
        case 2:
            add_record(&records, &record_count);
            break;
        case 3:
            search_records(records, record_count);
            break;
        case 4:
            update_record(records, record_count);
            break;
        case 5:
            delete_record(&records, &record_count);
            break;
        case 6:
            printf("Exiting...\n");
            return 0;
        default:
            printf("Invalid choice. Please try again.\n");
        }
    } while (num_choice != 6);

    free(records);
    return 0;
}

int display_menu()
{
    printf("Machine Maintenance Manager\n");
    printf("1. View Machine Maintenance\n");
    printf("2. Add record\n");
    printf("3. Search record\n");
    printf("4. Update record\n");
    printf("5. Delete record\n");
    printf("6. Exit\n\n");
    return 0;
}

static void read_line_prompt(const char *prompt, char *buf, size_t bufsz)
{
    printf("%s", prompt);
    if (fgets(buf, (int)bufsz, stdin) == NULL)
    {
        buf[0] = '\0';
        return;
    }
    trim_newline(buf);
    strip(buf);
}

static void trim_newline(char *s)
{
    if (!s)
        return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void strip(char *s)
{
    if (!s)
        return;
    size_t i = 0, n = strlen(s);
    while (i < n && isspace((unsigned char)s[i]))
        i++;
    if (i > 0)
        memmove(s, s + i, n - i + 1);
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
    {
        s[n - 1] = '\0';
        --n;
    }
}

int load_records(MaintenanceRecord **records, int *count)
{
    FILE *file = fopen(CSV_FILE, "r");
    if (!file)
    {
        perror("Failed to open file");
        return -1;
    }

    char line[LINE_BUF];
    int capacity = 10;
    *records = malloc(capacity * sizeof(MaintenanceRecord));
    if (!*records)
    {
        perror("Failed to allocate memory");
        fclose(file);
        return -1;
    }
    *count = 0;

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file))
    {
        trim_newline(line);
        strip(line);
        if (strlen(line) == 0)
            continue;

        char *token;
        token = strtok(line, ",");
        if (token)
            strncpy((*records)[*count].machineName, token, MAX_NAME);
        token = strtok(NULL, ",");
        if (token)
            strncpy((*records)[*count].machineID, token, MAX_ID);
        token = strtok(NULL, ",");
        if (token)
            strncpy((*records)[*count].maintenanceDate, token, MAX_DATE);
        token = strtok(NULL, ",");
        if (token)
            strncpy((*records)[*count].maintenanceDetails, token, MAX_DETAILS);

        (*count)++;
        if (*count >= capacity)
        {
            capacity *= 2;
            MaintenanceRecord *new_records = realloc(*records, capacity * sizeof(MaintenanceRecord));
            if (!new_records)
            {
                perror("Failed to reallocate memory");
                free(*records);
                fclose(file);
                return -1;
            }
            *records = new_records;
        }
    }

    fclose(file);
    return 0;
}

int display_records(MaintenanceRecord *records, int count)
{
    if (count == 0)
    {
        printf("No maintenance records available.\n");
        return 0;
    }

    printf("\n%-20s %-10s %-12s %-30s\n", "Machine Name", "Machine ID", "Date", "Details");
    printf("-------------------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++)
    {
        printf("%-20s %-10s %-12s %-30s\n",
               records[i].machineName,
               records[i].machineID,
               records[i].maintenanceDate,
               records[i].maintenanceDetails);
    }
    printf("\n");
    return 0;
}

int search_records(MaintenanceRecord *records, int count)
{
    char query[MAX_NAME];
    read_line_prompt("Enter machine name or ID to search: ", query, sizeof(query));
    if (strlen(query) == 0)
    {
        printf("Search query cannot be empty.\n");
        return -1;
    }

    int found = 0;
    printf("\nSearch Results:\n");
    printf("%-20s %-10s %-12s %-30s\n", "Machine Name", "Machine ID", "Date", "Details");
    printf("-------------------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++)
    {
        if (strstr(records[i].machineName, query) || strstr(records[i].machineID, query))
        {
            printf("%-20s %-10s %-12s %-30s\n",
                   records[i].machineName,
                   records[i].machineID,
                   records[i].maintenanceDate,
                   records[i].maintenanceDetails);
            found++;
        }
    }
    if (found == 0)
    {
        printf("No records found matching the query '%s'.\n", query);
    }
    else
    {
        printf("\nTotal %d record(s) found.\n", found);
    }
    return 0;
}

int add_record(MaintenanceRecord **records, int *count)
{
    MaintenanceRecord new_record;
    read_line_prompt("Enter machine name: ", new_record.machineName, sizeof(new_record.machineName));
    read_line_prompt("Enter machine ID: ", new_record.machineID, sizeof(new_record.machineID));
    read_line_prompt("Enter maintenance date (YYYY-MM-DD): ", new_record.maintenanceDate, sizeof(new_record.maintenanceDate));
    read_line_prompt("Enter maintenance details: ", new_record.maintenanceDetails, sizeof(new_record.maintenanceDetails));

    if (strlen(new_record.machineName) == 0 || strlen(new_record.machineID) == 0 ||
        strlen(new_record.maintenanceDate) == 0 || strlen(new_record.maintenanceDetails) == 0)
    {
        printf("All fields are required. Record not added.\n");
        return -1;
    }

    FILE *file = fopen(CSV_FILE, "a");
    if (!file)
    {
        perror("Failed to open file for appending");
        return -1;
    }
    fprintf(file, "%s,%s,%s,%s\n",
            new_record.machineName,
            new_record.machineID,
            new_record.maintenanceDate,
            new_record.maintenanceDetails);
    fclose(file);

    MaintenanceRecord *new_records = realloc(*records, (*count + 1) * sizeof(MaintenanceRecord));
    if (!new_records)
    {
        perror("Failed to allocate memory for new record");
        return -1;
    }
    *records = new_records;
    (*records)[*count] = new_record;
    (*count)++;

    printf("Record added successfully.\n");
    return 0;
}

int update_record(MaintenanceRecord *records, int count)
{
    char id[MAX_ID];
    read_line_prompt("Enter machine ID of the record to update: ", id, sizeof(id));
    if (strlen(id) == 0)
    {
        printf("Machine ID cannot be empty.\n");
        return -1;
    }

    for (int i = 0; i < count; i++)
    {
        if (strcmp(records[i].machineID, id) == 0)
        {
            printf("Current details:\n");
            printf("Machine Name: %s\n", records[i].machineName);
            printf("Maintenance Date: %s\n", records[i].maintenanceDate);
            printf("Maintenance Details: %s\n", records[i].maintenanceDetails);

            read_line_prompt("Enter new machine name (leave blank to keep current): ", records[i].machineName, sizeof(records[i].machineName));
            read_line_prompt("Enter new maintenance date (YYYY-MM-DD, leave blank to keep current): ", records[i].maintenanceDate, sizeof(records[i].maintenanceDate));
            read_line_prompt("Enter new maintenance details (leave blank to keep current): ", records[i].maintenanceDetails, sizeof(records[i].maintenanceDetails));

            FILE *file = fopen(CSV_FILE, "w");
            if (!file)
            {
                perror("Failed to open file for writing");
                return -1;
            }
            fprintf(file, "MachineName,MachineID,MaintenanceDate,MaintenanceDetails");
            for (int j = 0; j < count; j++)
            {
                fprintf(file, "%s,%s,%s,%s\n",
                        records[j].machineName,
                        records[j].machineID,
                        records[j].maintenanceDate,
                        records[j].maintenanceDetails);
            }
            fclose(file);

            printf("Record updated successfully.\n");
            return 0;
        }
    }

    printf("No record found with Machine ID '%s'.\n", id);
    return -1;
}

int delete_record(MaintenanceRecord **records, int *count)
{
    char id[MAX_ID];
    read_line_prompt("Enter machine ID of the record to delete: ", id, sizeof(id));
    if (strlen(id) == 0)
    {
        printf("Machine ID cannot be empty.\n");
        return -1;
    }

    for (int i = 0; i < *count; i++)
    {
        if (strcmp((*records)[i].machineID, id) == 0)
        {
            for (int j = i; j < *count - 1; j++)
            {
                (*records)[j] = (*records)[j + 1];
            }
            (*count)--;

            FILE *file = fopen(CSV_FILE, "w");
            if (!file)
            {
                perror("Failed to open file for writing");
                return -1;
            }
            fprintf(file, "MachineName,MachineID,MaintenanceDate,MaintenanceDetails");
            for (int j = 0; j < *count; j++)
            {
                fprintf(file, "%s,%s,%s,%s\n",
                        (*records)[j].machineName,
                        (*records)[j].machineID,
                        (*records)[j].maintenanceDate,
                        (*records)[j].maintenanceDetails);
            }
            fclose(file);

            MaintenanceRecord *new_records = realloc(*records, (*count) * sizeof(MaintenanceRecord));
            if (!new_records && *count > 0)
            {
                perror("Failed to reallocate memory after deletion");
                return -1;
            }
            *records = new_records;

            printf("Record deleted successfully.\n");
            return 0;
        }
    }

    printf("No record found with Machine ID '%s'.\n", id);
    return -1;
}


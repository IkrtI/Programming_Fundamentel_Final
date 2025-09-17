#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define CSV_FILE "maintenance.csv"

#define MAX_RECORDS 1000
#define MAX_NAME 64
#define MAX_ID 32
#define MAX_DATE 16
#define MAX_DETAILS 256

char machineName[MAX_RECORDS][MAX_NAME];
char machineID[MAX_RECORDS][MAX_ID];
char maintenanceDate[MAX_RECORDS][MAX_DATE];
char maintenanceDetails[MAX_RECORDS][MAX_DETAILS];
int record_count = 0;

void display_menu(void);
int ensure_csv_exists(void);
int load_records(void);
int save_all_records(void);
void display_records(void);
void add_record(void);
void search_records(void);
void update_record(void);
void delete_record(void);
int safe_input(char *buffer, int size, const char *prompt);

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
        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1)
        {
            int c;
            while ((c = getchar()) != '\n' && c != EOF)
            {
            }
            choice = 0;
        }

        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
        {
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

int ensure_csv_exists(void)
{
    FILE *f = fopen(CSV_FILE, "r");
    if (f)
    {
        fclose(f);
        return 0;
    }

    f = fopen(CSV_FILE, "w");
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
    FILE *file = fopen(CSV_FILE, "r");
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

        strncpy(machineName[record_count], n, MAX_NAME - 1);
        machineName[record_count][MAX_NAME - 1] = '\0';

        strncpy(machineID[record_count], id, MAX_ID - 1);
        machineID[record_count][MAX_ID - 1] = '\0';

        strncpy(maintenanceDate[record_count], d, MAX_DATE - 1);
        maintenanceDate[record_count][MAX_DATE - 1] = '\0';

        strncpy(maintenanceDetails[record_count], det, MAX_DETAILS - 1);
        maintenanceDetails[record_count][MAX_DETAILS - 1] = '\0';

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
    FILE *file = fopen(CSV_FILE, "w");
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

    if (safe_input(n, sizeof(n), "Enter machine name: ") != 0)
    {
        printf("Error reading machine name.\n");
        return;
    }

    if (safe_input(id, sizeof(id), "Enter machine ID: ") != 0)
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

    if (safe_input(d, sizeof(d), "Enter maintenance date (YYYY-MM-DD): ") != 0)
    {
        printf("Error reading maintenance date.\n");
        return;
    }

    if (safe_input(det, sizeof(det), "Enter maintenance details: ") != 0)
    {
        printf("Error reading maintenance details.\n");
        return;
    }

    strncpy(machineName[record_count], n, MAX_NAME - 1);
    machineName[record_count][MAX_NAME - 1] = '\0';

    strncpy(machineID[record_count], id, MAX_ID - 1);
    machineID[record_count][MAX_ID - 1] = '\0';

    strncpy(maintenanceDate[record_count], d, MAX_DATE - 1);
    maintenanceDate[record_count][MAX_DATE - 1] = '\0';

    strncpy(maintenanceDetails[record_count], det, MAX_DETAILS - 1);
    maintenanceDetails[record_count][MAX_DETAILS - 1] = '\0';

    record_count++;
    printf("Record added.\n");
}

void search_records(void)
{
    char q[64];
    int found = 0;

    if (safe_input(q, sizeof(q), "Enter machine name or ID to search: ") != 0)
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
    char buf[MAX_DETAILS];

    if (safe_input(id, sizeof(id), "Enter machine ID to update: ") != 0)
    {
        printf("Error reading machine ID.\n");
        return;
    }

    for (int i = 0; i < record_count; ++i)
    {
        if (strcmp(machineID[i], id) == 0)
        {
            printf("Current Name: %s\n", machineName[i]);
            printf("New name (leave blank to keep): ");
            buf[0] = '\0';
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                if (buf[0] != '\n' && buf[0] != '\0')
                {
                    buf[strcspn(buf, "\r\n")] = '\0';
                    strncpy(machineName[i], buf, MAX_NAME - 1);
                    machineName[i][MAX_NAME - 1] = '\0';
                }
            }

            printf("Current Date: %s\n", maintenanceDate[i]);
            printf("New date YYYY-MM-DD (leave blank to keep): ");
            buf[0] = '\0';
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                if (buf[0] != '\n' && buf[0] != '\0')
                {
                    buf[strcspn(buf, "\r\n")] = '\0';
                    strncpy(maintenanceDate[i], buf, MAX_DATE - 1);
                    maintenanceDate[i][MAX_DATE - 1] = '\0';
                }
            }

            printf("Current Details: %s\n", maintenanceDetails[i]);
            printf("New details (leave blank to keep): ");
            buf[0] = '\0';
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                if (buf[0] != '\n' && buf[0] != '\0')
                {
                    buf[strcspn(buf, "\r\n")] = '\0';
                    strncpy(maintenanceDetails[i], buf, MAX_DETAILS - 1);
                    maintenanceDetails[i][MAX_DETAILS - 1] = '\0';
                }
            }

            printf("Record updated.\n");
            return;
        }
    }
    printf("No record with Machine ID '%s'.\n", id);
}

void delete_record(void)
{
    char id[MAX_ID];
    
    if (safe_input(id, sizeof(id), "Enter machine ID to delete: ") != 0)
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

                strncpy(machineName[j], machineName[j + 1], MAX_NAME);
                strncpy(machineID[j], machineID[j + 1], MAX_ID);
                strncpy(maintenanceDate[j], maintenanceDate[j + 1], MAX_DATE);
                strncpy(maintenanceDetails[j], maintenanceDetails[j + 1], MAX_DETAILS);
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
    printf("%s", prompt);
    if (fgets(buffer, size, stdin) == NULL)
    {
        return -1;
    }
    
    // Remove trailing newline
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 0;
}

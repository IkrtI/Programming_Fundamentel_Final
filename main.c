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
// int load_records(MaintenanceRecord **records, int *count);
// int add_record(MaintenanceRecord **records, int *count);
// int search_records(MaintenanceRecord *records, int count);
// int update_record(MaintenanceRecord *records, int count);
// int delete_record(MaintenanceRecord **records, int *count);
// int sanitize_input(char *input);
// int display_records(MaintenanceRecord *records, int count);
static void read_line_prompt(const char *prompt, char *buffer, size_t size);
static void trim_newline(char *s);
static void strip(char *s);

int main()
{
    MaintenanceRecord *records = NULL;
    int record_count = 0;
    char choice[8];

    // load_records(&records, &record_count);

    int num_choice = 0;
    do
    {
        display_menu();
        read_line_prompt("Enter your choice: ", choice, sizeof(choice));
        num_choice = atoi(choice);
        switch (num_choice)
        {
        case 1:
            // display_records(records, record_count);
            break;
        case 2:
            // add_record(&records, &record_count);
            break;
        case 3:
            // search_records(records, record_count);
            break;
        case 4:
            // update_record(records, record_count);
            break;
        case 5:
            // delete_record(&records, &record_count);
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
    printf("2. Add Machine Maintenance\n");
    printf("3. Search Machine\n");
    printf("4. Update Machine\n");
    printf("5. Delete Machine\n");
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
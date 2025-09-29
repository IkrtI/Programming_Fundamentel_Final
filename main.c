#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>

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
#ifndef UNIT_TEST
static int copy_csv_file(const char *source, const char *destination);
static int show_csv_control_menu(void);
static int read_csv_menu_choice(size_t csv_count, int *selected_index, char *command);
static int prompt_csv_index_selection(size_t csv_count, const char *prompt, int *out_index);
static int has_csv_extension(const char *name);
static int is_regular_file(const char *path, const struct dirent *entry);
static int compare_csv_names(const void *lhs, const void *rhs);
#endif

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

#ifndef UNIT_TEST
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
        if (status == INPUT_CANCELLED)
        {
            print_cancel_message();
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
        int status = safe_input(buffer, sizeof(buffer), "Select CSV option: ");
        if (status == INPUT_CANCELLED)
        {
            print_cancel_message();
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
            printf("Invalid choice. Enter a number or A/N/C/Q.\n");
            continue;
        }

        if (isalpha((unsigned char)buffer[0]) && buffer[1] == '\0')
        {
            char c = (char)toupper((unsigned char)buffer[0]);
            if (c == 'A' || c == 'N' || c == 'C' || c == 'Q')
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
            printf("Invalid choice. Enter a number between 1 and %zu or A/N/C/Q.\n", csv_count);
        }
        else
        {
            printf("Invalid choice. Enter A/N/C/Q.\n");
        }
    }
}

static int show_csv_control_menu(void)
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

    const char *current_path = maintenance_get_csv_path();
    printf("\nCSV File Management\n");
    for (size_t i = 0; i < csv_count; ++i)
    {
        printf("%zu. %s\n", i + 1, csv_files[i]);
    }

    printf("A. Enter CSV file path manually\n");
    printf("N. Create a new blank CSV file\n");
    if (csv_count > 0)
    {
        printf("C. Copy an existing CSV file\n");
    }
    printf("Q. Keep current CSV (%s)\n", current_path && current_path[0] != '\0' ? current_path : "default");

    int result = 1;

    while (!interrupt_requested)
    {
        int selected_index = -1;
        char command = '\0';
        int status = read_csv_menu_choice(csv_count, &selected_index, &command);
        if (status == INPUT_CANCELLED)
        {
            break;
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
            if (input_status == INPUT_CANCELLED)
            {
                print_cancel_message();
                break;
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
            if (input_status == INPUT_CANCELLED)
            {
                print_cancel_message();
                break;
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
        else if (command == 'C')
        {
            if (csv_count == 0)
            {
                printf("No CSV files available to copy.\n");
                continue;
            }

            int source_index = -1;
            int index_status = prompt_csv_index_selection(csv_count, "Enter the number of the CSV to copy: ", &source_index);
            if (index_status == INPUT_CANCELLED)
            {
                break;
            }

            if (index_status == INPUT_ERROR)
            {
                printf("Input error detected. Leaving CSV menu.\n");
                result = -1;
                break;
            }

            char destination[CSV_PATH_MAX];
            int dest_status = safe_input(destination, sizeof(destination), "Enter name for the new CSV file: ");
            if (dest_status == INPUT_CANCELLED)
            {
                print_cancel_message();
                break;
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
                printf("Failed to copy '%s' to '%s'.\n", csv_files[source_index], destination);
                continue;
            }

            if (maintenance_set_csv_path(destination) != 0)
            {
                printf("Failed to set CSV path to '%s'.\n", destination);
                continue;
            }

            printf("Copied '%s' to '%s'.\n", csv_files[source_index], destination);
            reload_records_with_warning();
            result = 0;
            break;
        }
        else if (command == 'Q')
        {
            if (current_path && current_path[0] != '\0')
            {
                printf("Keeping existing CSV file: %s\n", current_path);
            }
            else
            {
                printf("Keeping existing CSV file configuration.\n");
            }
            result = 1;
            break;
        }
    }

    for (size_t i = 0; i < csv_count; ++i)
    {
        free(csv_files[i]);
    }
    free(csv_files);

    if (interrupt_requested)
    {
        return -1;
    }

    return result;
}

#endif /* UNIT_TEST */

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

    signal(SIGINT, handle_sigint);

    int csv_menu_status = show_csv_control_menu();
    if (csv_menu_status < 0)
    {
        return 1;
    }

    if (interrupt_requested)
    {
        clear_record_storage();
        printf("\nCtrl+C detected. Cleared maintenance data from memory before exiting.\n");
        return 1;
    }

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
            printf("Invalid choice. Please enter a number between 1 and 7.\n");
            continue;
        }

        char *endptr = NULL;
        long value = strtol(buffer, &endptr, 10);
        if (endptr != NULL && *endptr == '\0' && value >= 1 && value <= 7)
        {
            return (int)value;
        }

        printf("Invalid choice. Please enter a number between 1 and 7.\n");
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
    system("cls");
#else
    system("clear");
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

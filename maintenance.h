#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <stddef.h>

#ifndef CSV_FILE_DEFAULT
#define CSV_FILE_DEFAULT "maintenance.csv"
#endif

#ifndef CSV_PATH_MAX
#define CSV_PATH_MAX 512
#endif

#define MAX_RECORDS 1000
#define MAX_NAME 64
#define MAX_ID 32
#define MAX_DATE 16
#define MAX_DETAILS 256

int ensure_csv_exists(void);
int load_records(void);
int save_all_records(void);
void display_records(void);
void add_record(void);
void search_records(void);
void update_record(void);
void delete_record(void);
int safe_input(char *buffer, int size, const char *prompt);
int prompt_with_validation(char *buffer, int size, const char *prompt,
                           int (*validator)(const char *), const char *error_message);
void trim_whitespace(char *str);
void sanitize_input(char *buffer);
int is_non_empty(const char *str);
int is_valid_machine_name(const char *str);
int is_valid_machine_id(const char *str);
int is_valid_date(const char *str);
int is_valid_details(const char *str);
int contains_disallowed_csv_chars(const char *str);
void prompt_optional_update(const char *prompt, char *dest, int dest_size,
                            int (*validator)(const char *), const char *error_message);
int maintenance_set_csv_path(const char *path);
const char *maintenance_get_csv_path(void);

extern int record_count;

#endif /* MAINTENANCE_H */

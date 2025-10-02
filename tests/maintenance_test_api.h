#ifndef MAINTENANCE_TEST_API_H
#define MAINTENANCE_TEST_API_H

#include "../maintenance.h"

/* Accessors */
const char *maintenance_get_csv_path(void);
int maintenance_set_csv_path(const char *path);
int is_record_storage_full(void);

/* Persistence */
int ensure_csv_exists(void);
int load_records(void);
int save_all_records(void);
int reload_records_with_warning(void);

/* Validation helpers */
void trim_whitespace(char *str);
void sanitize_input(char *buffer);
int is_non_empty(const char *str);
int contains_disallowed_csv_chars(const char *str);
int contains_cancel_signal(const char *str);
int is_valid_machine_name(const char *str);
int is_valid_machine_id(const char *str);
int is_valid_date(const char *str);
int is_valid_details(const char *str);

/* Core operations */
void display_records(void);
void add_record(void);
void search_records(void);
void update_record(void);
void delete_record(void);
void manage_deleted_records(void);

/* Shared globals exposed only for tests */
extern int record_count;

#endif /* MAINTENANCE_TEST_API_H */

# Machine Maintenance Manager (C, CSV CRUD)

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

## Working Effectively

- **Build the application:**
  - `gcc -std=c11 -Wall -Wextra -O2 -o maint main.c` -- builds in under 1 second. NEVER CANCEL.
  - Alternative: `./run.sh` -- builds and runs the application directly
  - The build produces compiler warnings about unused return values and string truncation, but these are non-critical and the application works correctly

- **Run the application:**
  - `./maint` -- starts the interactive menu-driven application
  - Alternative: `./run.sh` -- builds and runs in one command

- **Test the application:**
  - ALWAYS run through complete user scenarios after making changes
  - Test all menu options: View (1), Add (2), Search (3), Update (4), Delete (5), Exit (6)
  - Verify CSV file creation and data persistence
  - Test with both empty database and existing records

## Validation

- **ALWAYS manually validate any code changes by running complete user scenarios**
- **Required validation steps after changes:**
  1. Build the application with `gcc -std=c11 -Wall -Wextra -O2 -o maint main.c`
  2. Start the application with `./maint`
  3. Test adding a new record (option 2): provide machine name, unique ID, date (YYYY-MM-DD), and details
  4. Test viewing records (option 1): verify the new record appears correctly formatted
  5. Test searching (option 3): search by machine name or ID to verify search functionality
  6. Test updating a record (option 4): modify an existing record by machine ID
  7. Test deleting a record (option 5): remove a record by machine ID
  8. Verify CSV file (`maintenance.csv`) is created/updated correctly after each operation
  9. Exit application (option 6) and restart to verify data persistence

- **CSV file validation:**
  - Check that `maintenance.csv` exists after first add operation
  - Verify CSV header: `MachineName,MachineID,MaintenanceDate,MaintenanceDetails`
  - Confirm data format matches header structure
  - Test with existing data by using `maintenance-example.csv` as input

## Build Details

- **Build time:** Under 1 second with GCC
- **Dependencies:** Standard C library only, no external dependencies required
- **Compiler:** GCC with C11 standard
- **Build warnings:** Expected warnings about scanf return values and strncpy truncation - these are non-critical
- **Output binary:** `maint` (or `MachineManager` when using run.sh)

## Application Architecture

- **Single file C application:** `main.c`
- **Data storage:** CSV file (`maintenance.csv`) in working directory
- **Memory storage:** In-memory arrays for runtime operations (MAX_RECORDS = 1000)
- **Data reloading:** CSV file is reloaded on each menu loop iteration
- **Key fields:**
  - MachineName (max 64 chars)
  - MachineID (max 32 chars, must be unique)
  - MaintenanceDate (max 16 chars, format YYYY-MM-DD)
  - MaintenanceDetails (max 256 chars)

## File Structure and Key Locations

- **Main application:** `main.c` - contains all application logic
- **Build script:** `run.sh` - convenience script for build and run
- **Example data:** `maintenance-example.csv` - sample data for testing
- **Documentation:** `README.md` - contains Thai language documentation and usage examples
- **License:** `LICENSE` - Mozilla Public License 2.0

## Available Tools

- **GCC compiler:** `/usr/bin/gcc` - primary build tool
- **Make:** `/usr/bin/make` - available but not used by this project
- **clang-format:** `/usr/bin/clang-format` - available for code formatting if needed

## Common Issues and Solutions

- **Missing CSV file:** Application automatically creates `maintenance.csv` with proper headers on first run
- **Build warnings:** Compiler warnings about scanf and strncpy are expected and do not affect functionality
- **Duplicate machine IDs:** Application prevents adding records with duplicate machine IDs
- **Data persistence:** All changes are immediately written to CSV file for operations 2, 4, and 5
- **Maximum records:** Application supports up to 1000 maintenance records

## Testing Scenarios

- **Empty database test:** Start with no CSV file, add first record, verify file creation
- **Data persistence test:** Add record, exit application, restart, verify record exists
- **Search functionality test:** Add multiple records, search by partial machine name or ID
- **Update operation test:** Modify existing record fields, verify changes persist
- **Delete operation test:** Remove record, verify it no longer appears in listings
- **CSV format test:** Manually inspect `maintenance.csv` to verify proper CSV formatting

## Repository Commands Reference

### List files in repository root
```
ls -la
```
Output:
```
.git
.gitignore
LICENSE
README.md
main.c
maintenance-example.csv
run.sh
```

### View README.md content
The README.md contains Thai language documentation with build instructions, CSV schema, and design notes explaining the application's CRUD operations and CSV handling approach.

### View main.c structure
The main.c file contains:
- Header includes and constants definitions
- Global arrays for storing maintenance records
- Function declarations for all operations
- Main menu loop with choice handling
- CRUD operation implementations
- CSV file I/O functions
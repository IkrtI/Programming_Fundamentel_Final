<h1 align="center">
Machine Maintenance Manager
</h1>
<p align="center">
ระบบจัดการประวัติการบำรุงรักษาเครื่องจักร (ภาษา C) พร้อมตัวอย่างข้อมูล CSV และชุดทดสอบอัตโนมัติ
</p>

<p align="center">
  <img src="https://github.com/IkrtI/Programming_Fundamentel_Final/actions/workflows/ci.yml/badge.svg?branch=main">
  <img src="https://img.shields.io/github/license/ikrti/Programming_Fundamentel_Final?logo=github&labelColor=%23404040&color=%234f004f">
  <img src="https://img.shields.io/github/issues/ikrti/Programming_Fundamentel_Final?logo=github&labelColor=%23404040&color=%234f004f">
  <img src="https://img.shields.io/github/languages/top/ikrti/Programming_Fundamentel_Final?logo=github&labelColor=%23404040&color=%234f004f">
</p><p align="center">
  <img src="https://img.shields.io/github/repo-size/ikrti/Programming_Fundamentel_Final?logo=github&labelColor=%23404040&color=%234f004f">
  <img src="https://img.shields.io/github/last-commit/ikrti/Programming_Fundamentel_Final?logo=github&labelColor=%23404040&color=%234f004f">
  <img src="https://img.shields.io/github/commit-activity/m/ikrti/Programming_Fundamentel_Final?logo=github&labelColor=%23404040&color=%234f004f">
</p>

----

## 📚 Table of Contents

- [Quick Start](#-quick-start)
  - [Build](#build)
  - [Run](#run)
- [User Interface Preview](#%EF%B8%8F-user-interface-preview)
- [CSV Schema & Sample Data](#%EF%B8%8F-csv-schema--sample-data)

## ⚡ Quick Start

### Build

สำหรับ Linux / macOS สามารถใช้สคริปต์ `COMPILE` เพื่อสร้างทั้งไบนารีหลักและชุดทดสอบภายในในคำสั่งเดียว:

```bash
./COMPILE
```

บน Windows ให้เรียก `COMPILE.bat` แทน (ภายในจะใช้ MinGW-w64 หรือ MSYS2 GCC):

```bat
COMPILE.bat
```

หากต้องการคอมไพล์เองด้วยคำสั่งเดิมก็ยังใช้ได้:

```bash
gcc -std=c11 -Wall -Wextra -O2 -o maint main.c
```

ทั้งสองสคริปต์จะสร้างไฟล์:

- `maint` หรือ `maint.exe` สำหรับรันโปรแกรมปกติ
- `maint_tests` หรือ `maint_tests.exe` สำหรับรันชุดทดสอบในโปรเซสเดียวกับโปรแกรมหลัก

### Run

```bash
./maint         # หรือ maint.exe บน Windows
```

### Run automated tests

เมื่อมีไฟล์ `maint_tests` แล้ว สามารถรันชุดทดสอบได้โดยตรง (ไม่ต้องเรียกสคริปต์ภายนอก):

```bash
./maint_tests --run-unit-tests
./maint_tests --run-e2e-tests
```

ยังคงมีสคริปต์ `tests/run_unit_tests.*` และ `tests/run_e2e_tests.*` ให้ใช้ใน CI หรือการพัฒนา ซึ่งจะเรียกคอมไพล์ด้วยแฟล็กที่จำเป็นให้อัตโนมัติ

เมื่อรันโปรแกรมจะพบเมนูจัดการไฟล์ CSV ก่อนเข้าสู่เมนูหลัก:

- โปรแกรมจะแสดงรายชื่อไฟล์ `*.csv` ใน working directory พร้อมหมายเลข 1, 2, 3, ...
- พิมพ์หมายเลขเพื่อเลือกไฟล์, กด `A` เพื่อระบุพาธเอง, `N` เพื่อสร้างไฟล์ใหม่จากหัวตารางว่าง, หรือ `C` เพื่อคัดลอกไฟล์ที่เลือกไปยังชื่อใหม่
- กด `Q` (หรือกด Ctrl+X เพื่อยกเลิกอินพุต) เพื่อคงค่าไฟล์เดิมและเข้าสู่เมนูหลัก
- เมื่อสร้างหรือคัดลอกไฟล์ใหม่ โปรแกรมจะโหลดข้อมูลล่าสุดให้อัตโนมัติ

## 🖥️ User Interface Preview

เมื่อรันโปรแกรม คุณจะพบเมนูหลักดังนี้:

> CSV File Management
```
CSV File Management

CSV current path: maintenance.csv

1. maintenance.csv

Other
C. Continue with "maintenance.csv" CSV file
A. Enter CSV file path manually
N. Create a new blank CSV file
D. Duplicate an existing CSV file
Q. Exit
Select CSV option : 
```

> Machine Maintenance Manager
```
Machine Maintenance Manager
1. View Machine Maintenance
2. Add record
3. Search record
4. Update record
5. Delete record
6. Manage deleted records
7. Add-on tools
8. Test program
9. Exit
Enter your choice : 
```

## 🗂️ CSV Schema & Sample Data

- **ชื่อไฟล์หลัก:** `maintenance.csv` (ต้องอยู่ใน working directory)
- **บรรทัดหัวตาราง:**

  ```csv
  MachineName,MachineID,MaintenanceDate,MaintenanceDetails,Active
  ```

- **ตัวอย่างข้อมูล:**

  ```csv
  Hydraulic Press,HP-001,2025-08-01,Changed hydraulic fluid,1
  CNC Mill,CNC-12,2025-08-03,Spindle alignment and lubrication,1
  Laser Cutter,LC-07,2025-08-05,Replaced air filter,1
  ```

> หมายเหตุ: โครงสร้าง CSV ต้องตรงตามตัวอย่างเพื่อให้โปรแกรมอ่านและเขียนข้อมูลได้ถูกต้อง

## 🚀 Continuous Integration & Releases

รีลีสที่เผยแพร่บน GitHub จะถูกสร้างผ่าน workflow อัตโนมัติที่คอมไพล์ไบนารีสำหรับ Linux, macOS และ Windows จากนั้นรันทั้งชุด **unit tests** และ **end-to-end tests** โดยบันทึกผลลัพธ์ไว้ในไฟล์ `.txt` ต่อแพลตฟอร์ม ก่อนจะแนบไฟล์ไบนารีและผลการทดสอบทั้งหมดเข้าไปในรีลีสเดียวกัน คุณจึงสามารถตรวจสอบทั้ง build artifact และหลักฐานการทดสอบของแต่ละเวอร์ชันได้จากหน้ารีลีสทันที

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

สำหรับ Linux / macOS ให้ใช้สคริปต์ `build.sh` ที่เตรียมไว้ (สคริปต์จะคอมไพล์พร้อมแฟล็ก `-DENABLE_INTERNAL_TESTS` เพื่อเปิดเมนู **Test program** และใช้งานไบนารี `maint_tests` ได้ทันที):

```bash
./build.sh
```

บน Windows ใช้ `build.bat` ซึ่งตั้งค่าพารามิเตอร์มาตรฐาน (รวมแฟล็ก `-DENABLE_INTERNAL_TESTS`) ให้แล้ว:

```bat
build.bat
```

หากต้องการคอมไพล์เองด้วยคำสั่งเดิมก็ยังใช้ได้ (อย่าลืมใส่ `-DENABLE_INTERNAL_TESTS` เพื่อให้เมนูทดสอบพร้อมใช้งาน):

```bash
gcc -std=c11 -Wall -Wextra -O2 -DENABLE_INTERNAL_TESTS -I. -o maint main.c
```

ค่าเริ่มต้นจะสร้างไฟล์ปฏิบัติการหลัก `maint` (หรือ `maint.exe`)

### Run

```bash
./maint         # หรือ maint.exe บน Windows
```

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
6. Add-on tools
---------
7. Run unit tests
8. Run end-to-end tests
9. Exit
Enter your choice : 
```

## 🗂️ CSV Schema & Sample Data

- **ชื่อไฟล์หลัก:** `maintenance.csv` (ต้องอยู่ใน working directory)
- **บรรทัดหัวตาราง:**

  ```csv
  MachineName,MachineID,MaintenanceDate,MaintenanceDetails
  ```

- **ตัวอย่างข้อมูล:**

  ```csv
  Hydraulic Press,HP-001,2025-08-01,Changed hydraulic fluid
  CNC Mill,CNC-12,2025-08-03,Spindle alignment and lubrication
  Laser Cutter,LC-07,2025-08-05,Replaced air filter
  ```

> หมายเหตุ: โครงสร้าง CSV ต้องตรงตามตัวอย่างเพื่อให้โปรแกรมอ่านและเขียนข้อมูลได้ถูกต้อง
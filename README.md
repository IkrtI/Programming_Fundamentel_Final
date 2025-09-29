# Machine Maintenance Manager
[![CI](https://github.com/IkrtI/Programming_Fundamentel_Final/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/IkrtI/Programming_Fundamentel_Final/actions/workflows/ci.yml)

> ระบบจัดการประวัติการบำรุงรักษาเครื่องจักร (ภาษา C) พร้อมตัวอย่างข้อมูล CSV และชุดทดสอบอัตโนมัติ

## 📚 Table of Contents

- [Quick Start](#-quick-start)
  - [Build](#build)
  - [Run](#run)
- [User Interface Preview](#%EF%B8%8F-user-interface-preview)
- [CSV Schema & Sample Data](#%EF%B8%8F-csv-schema--sample-data)

## ⚡ Quick Start

### Build

คอมไพล์ด้วย GCC (รองรับ Linux / macOS / Windows):

```bash
gcc -std=c11 -Wall -Wextra -O2 -o maint main.c
```

### Run

```bash
./maint         # หรือ maint.exe บน Windows
```

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
6. Add-on tools
7. Test program
8. Exit
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

[![CI](https://github.com/IkrtI/Programming_Fundamentel_Final/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/IkrtI/Programming_Fundamentel_Final/actions/workflows/ci.yml)

# Machine Maintenance Manager

> ระบบจัดการประวัติการบำรุงรักษาเครื่องจักร (ภาษา C) พร้อมตัวอย่างข้อมูล CSV และชุดทดสอบอัตโนมัติ

## 📚 Table of Contents

- [Quick Start](#-quick-start)
  - [Build](#build)
  - [Test](#test)
  - [Run](#run)
- [User Interface Preview](#-user-interface-preview)
- [CSV Schema & Sample Data](#-csv-schema--sample-data)
- [Design Notes](#-design-notes)

## ⚡ Quick Start

### Build

คอมไพล์ด้วย GCC (รองรับ Linux / macOS / Windows):

```bash
gcc -std=c11 -Wall -Wextra -O2 -o maint main.c
```

### Test

รันทดสอบแบบหน่วย (unit test):

```bash
gcc -std=c11 -Wall -Wextra -O2 -DEND_TO_END_TEST -DUNIT_TEST -I. \
    -o tests/test_end_to_end tests/test_end_to_end.c main.c
./tests/test_end_to_end
```

รันทดสอบแบบ End-To-End Test:

```bash
gcc -std=c11 -Wall -Wextra -O2 -DEND_TO_END_TEST -I. -o tests/test_end_to_end tests/test_end_to_end.c main.c
./tests/test_end_to_end
```

### Run

```bash
./maint         # หรือ maint.exe บน Windows
```

## 🖥️ User Interface Preview

เมื่อรันโปรแกรม คุณจะพบเมนูหลักดังนี้:

```
Machine Maintenance Manager
1. View Machine Maintenance
2. Add record
3. Search record
4. Update record
5. Delete record
6. Exit
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

## 🧭 Design Notes

- **โหลดข้อมูล:** ทุกครั้งที่แสดงเมนู โปรแกรมจะอ่านข้อมูลจาก `maintenance.csv` ใหม่
- **เพิ่มข้อมูล:** การเพิ่มบันทึกจะเขียนแถวใหม่ต่อท้ายไฟล์ CSV
- **อัปเดต / ลบ:** สร้างไฟล์ชั่วคราวแล้วเขียนข้อมูลใหม่ทั้งไฟล์ (รวม header)
- **ตรวจสอบความซ้ำ:** `MachineID` ต้องไม่ซ้ำกัน โดยโปรแกรมจะตรวจเช็คก่อนเพิ่มข้อมูลใหม่
- **ค้นหา:** ใช้การค้นหาแบบ case-sensitive ด้วย `strstr` ใน `MachineName` และ `MachineID`
- **การอัปเดต:** ถ้ากรอกค่าว่างระหว่างอัปเดต ค่านั้นจะถูกบันทึกเป็นว่าง (ยังไม่รองรับการเว้นว่างเพื่อรักษาค่าเดิม)

พร้อมใช้งานทั้งภาษาไทยและอังกฤษ เหมาะสำหรับการทดลองสร้างระบบ CRUD ด้วยภาษา C และไฟล์ CSV 🛠️

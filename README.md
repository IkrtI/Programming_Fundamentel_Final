# Machine Maintenance Manager (C, CSV CRUD)

## 🚀 Get started

### Build

**Linux/macOS/Windows (GCC):**
```bash
gcc -std=c11 -Wall -Wextra -O2 -o maint main.c
```

### Test

```bash
gcc -std=c11 -Wall -Wextra -O2 -DUNIT_TEST -I. -o tests/test_validation tests/test_validation.c main.c
./tests/test_validation
```

### Run

```bash
./maint         # หรือ maint.exe บน Windows
```

เมนูจะมีลักษณะดังนี้

```
Machine Maintenance Manager
1. View Machine Maintenance
2. Add record
3. Search record
4. Update record
5. Delete record
6. Exit
```

---

## 🗂️ สคีมาของ CSV และตัวอย่างไฟล์

**ชื่อไฟล์**: `maintenance.csv` (ต้องอยู่ในไดเรกทอรีทำงาน)  
**บรรทัดหัวตาราง (จำเป็นต้องมี):**
```
MachineName,MachineID,MaintenanceDate,MaintenanceDetails
```

**ตัวอย่างข้อมูล:**
```
Hydraulic Press,HP-001,2025-08-01,Changed hydraulic fluid
CNC Mill,CNC-12,2025-08-03,Spindle alignment and lubrication
Laser Cutter,LC-07,2025-08-05,Replaced air filter
```

---

## 🧭 หลักการทำงาน (Design Notes)

- **โหลดข้อมูล**: ในทุก ๆ รอบลูปของเมนู โปรแกรมจะอ่าน `maintenance.csv` เข้าหน่วยความจำใหม่
- **เพิ่ม (Add)**: เขียนบรรทัดใหม่ต่อท้าย `maintenance.csv`
- **อัปเดต/ลบ**: เขียนไฟล์ CSV ใหม่ทั้งไฟล์ (พร้อมหัวตาราง) หลังแก้ไข
- **ทำความสะอาดอินพุต**: อ่านด้วย `fgets` แล้วตัด `\r\n` และเว้นวรรคหัว-ท้าย ด้วย `trim_newline` และ `strip`
- **เอกลักษณ์**: ใช้ `MachineID` เป็นคีย์ที่ต้องไม่ซ้ำ (ตรวจตอนเพิ่ม)
- **ค้นหา**: ค้นหาแบบ case-sensitive ด้วย `strstr` ใน `MachineName` และ `MachineID`
- **พฤติกรรมการอัปเดต**: ณ ตอนนี้หากกรอกค่าว่างในการอัปเดต ฟิลด์นั้นจะถูกบันทึกเป็นว่าง (ยังไม่รองรับ “ปล่อยว่างเพื่อคงค่าเดิม”)


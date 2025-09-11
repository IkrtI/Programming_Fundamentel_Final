# Machine Maintenance Manager (C, CSV CRUD)

แอปคอนโซลขนาดเล็ก (ข้ามแพลตฟอร์ม) สำหรับ **ดู/เพิ่ม/ค้นหา/อัปเดต/ลบ** ข้อมูลการบำรุงรักษาเครื่องจักรที่บันทึกในไฟล์ CSV

---

## ✨ ความสามารถ (Features)

- แสดงรายการข้อมูลบำรุงรักษาเป็นตาราง
- เพิ่มรายการใหม่
- ค้นหาจาก **ชื่อเครื่อง (MachineName)** หรือ **รหัสเครื่อง (MachineID)**
- อัปเดตรายการด้วย `MachineID`
- ลบรายการด้วย `MachineID`

---

## 🚀 Get started

### Build

**Linux/macOS/Windows (GCC):**
```bash
gcc -std=c11 -O2 -Wall -Wextra -o MachineManager main.c

```

### Run
```sh
./MachineManager         # หรือ MachineManager.exe บน Windows
```

---

# HOW TO COMPILE

เอกสารนี้อธิบายขั้นตอนการคอมไพล์โปรแกรม Machine Maintenance Manager จากซอร์สโค้ดหลัก `main.c` บนแพลตฟอร์มยอดนิยม

## 1. เตรียมเครื่องมือที่จำเป็น

| แพลตฟอร์ม | คอมไพเลอร์ที่แนะนำ | วิธีติดตั้ง |
|-----------|---------------------|-------------|
| Linux / macOS | [GCC](https://gcc.gnu.org/) หรือ [Clang](https://clang.llvm.org/) | ใช้ตัวจัดการแพ็กเกจ เช่น `sudo apt install build-essential`, `brew install gcc` |
| Windows | [MinGW-w64](http://mingw-w64.org/) หรือ [MSYS2](https://www.msys2.org/) | ติดตั้ง MSYS2 แล้วรัน `pacman -S --needed base-devel mingw-w64-x86_64-gcc` |

> หมายเหตุ: ต้องมีเครื่องมือบรรทัดคำสั่ง (เช่น Terminal, PowerShell, หรือ MSYS2 MinGW shell) เพื่อรันคำสั่งคอมไพล์

## 2. โครงสร้างไฟล์ที่ใช้คอมไพล์

ซอร์สโค้ดหลักอยู่ในไฟล์ `main.c` และมีส่วนหัวที่จำเป็นใน `maintenance.h` โดยไม่มีไลบรารีภายนอกเพิ่มเติม ดังนั้นสามารถคอมไพล์ได้ด้วยคำสั่งเดียว

```
project/
├── main.c
├── maintenance.h
└── ...
```

## 3. คำสั่งคอมไพล์มาตรฐาน

รันคำสั่งต่อไปนี้จากรูทโปรเจกต์ (โฟลเดอร์เดียวกับ `main.c`):

```bash
gcc -std=c11 -Wall -Wextra -O2 -o maint main.c
```

หากใช้ Clang แทน GCC สามารถสลับชื่อคอมไพเลอร์:

```bash
clang -std=c11 -Wall -Wextra -O2 -o maint main.c
```

หรือใช้สคริปต์ `COMPILE` / `COMPILE.bat` ที่เตรียมไว้เพื่อสร้างทั้งไบนารีหลักและไฟล์ทดสอบภายใน:

```bash
./COMPILE          # Linux / macOS
COMPILE.bat        # Windows
```

### ตัวเลือกเพิ่มเติม (ไม่บังคับ)
- เพิ่ม `-g` หากต้องการ debug symbols
- เพิ่ม `-fsanitize=address` (เฉพาะ Linux/macOS) เพื่อตรวจจับ memory error ระหว่างพัฒนา

## 4. การคอมไพล์บน Windows (MinGW-w64)

เปิด *MSYS2 MinGW 64-bit* shell แล้วรัน:

```bash
x86_64-w64-mingw32-gcc -std=c11 -Wall -Wextra -O2 -o maint.exe main.c
```

ไฟล์ปฏิบัติการที่ได้คือ `maint.exe`

## 5. การคอมไพล์แบบข้ามแพลตฟอร์มผ่าน CMake (ทางเลือก)

หากต้องการระบบ build ที่ยืดหยุ่น สามารถใช้ CMake ควบคู่กับ GCC/Clang:

1. สร้างไฟล์ `CMakeLists.txt` (มีตัวอย่างใน README) หรือใช้ตัวอย่างด้านล่าง:
   ```cmake
   cmake_minimum_required(VERSION 3.16)
   project(MaintenanceManager C)
   set(CMAKE_C_STANDARD 11)
   add_executable(maint main.c)
   target_compile_options(maint PRIVATE -Wall -Wextra)
   ```
2. รันคำสั่ง
   ```bash
   cmake -S . -B build
   cmake --build build
   ```
3. ไฟล์ปฏิบัติการจะอยู่ใน `build/maint` (หรือ `build/Debug/maint.exe` บน Windows)

## 6. ทดสอบหลังคอมไพล์

หลังคอมไพล์เสร็จสามารถรันโปรแกรมด้วยคำสั่ง:

```bash
./maint          # บน Linux / macOS
maint.exe        # บน Windows
```

ต้องมีไฟล์ `maintenance.csv` หรือสร้างไฟล์ใหม่ผ่านเมนูในโปรแกรม

### การรันทดสอบภายใน

เมื่อใช้สคริปต์ `COMPILE` / `COMPILE.bat` จะได้ไฟล์ `maint_tests` (หรือ `maint_tests.exe`) สำหรับรันทดสอบในโปรเซสเดียวกับโปรแกรมหลัก:

```bash
./maint_tests --run-unit-tests
./maint_tests --run-e2e-tests
```

ยังสามารถใช้สคริปต์ดั้งเดิมในโฟลเดอร์ `tests/` ซึ่งจะคอมไพล์ด้วยแฟล็กทดสอบให้โดยอัตโนมัติ:

```bash
./tests/run_unit_tests.sh
./tests/run_e2e_tests.sh
```

## 7. แก้ไขปัญหาที่พบบ่อย

| อาการ | วิธีแก้ |
|-------|---------|
| `gcc: command not found` | ติดตั้ง GCC หรือเพิ่ม path ของคอมไพเลอร์ในระบบ |
| Error เกี่ยวกับ encoding หรือ locale | ตั้งค่า locale เป็น UTF-8 (`export LC_ALL=C.UTF-8`) ก่อนคอมไพล์ |
| เตือนเกี่ยวกับ `deprecated` หรือ `implicit declaration` | ตรวจสอบให้แน่ใจว่าใช้มาตรฐาน `-std=c11` และประกาศฟังก์ชันใน `maintenance.h` ครบ |

เมื่อทำครบตามขั้นตอนข้างต้น คุณจะได้ไฟล์ปฏิบัติการพร้อมใช้งานบนแพลตฟอร์มของคุณ

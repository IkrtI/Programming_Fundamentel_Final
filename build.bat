@echo off
setlocal
set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

if "%CC%"=="" (
    set "CC=gcc"
)
if "%CFLAGS%"=="" (
    set "CFLAGS=-std=c11 -Wall -Wextra -O2"
)
set "EXTRA_DEFINES=-DENABLE_INTERNAL_TESTS"

set "OUTPUT=%SCRIPT_DIR%\maint.exe"
set "SRC=%SCRIPT_DIR%\main.c"
set "INCLUDE=-I%SCRIPT_DIR%"

echo Building maint with %CC% %CFLAGS% %EXTRA_DEFINES%
"%CC%" %CFLAGS% %EXTRA_DEFINES% %INCLUDE% -o "%OUTPUT%" "%SRC%"
if errorlevel 1 exit /b 1
echo Output: %OUTPUT%

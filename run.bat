@echo off
setlocal
set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

call "%SCRIPT_DIR%\COMPILE.bat"
if errorlevel 1 exit /b 1

"%SCRIPT_DIR%\maint.exe"

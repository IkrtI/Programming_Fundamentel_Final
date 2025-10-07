@echo off
setlocal
set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

pushd "%SCRIPT_DIR%" >nul

echo [build] maint (release)
gcc -std=c11 -O2 -Wall -Wextra -o maint.exe main.c
if errorlevel 1 goto :error

echo [build] maint_tests (internal tests)
gcc -std=c11 -O2 -Wall -Wextra -DENABLE_INTERNAL_TESTS -I"%SCRIPT_DIR%" -o maint_tests.exe main.c
if errorlevel 1 goto :error

echo Build artifacts:
echo   maint.exe
echo   maint_tests.exe
popd >nul
exit /b 0

:error
popd >nul
exit /b 1

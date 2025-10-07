@echo off
setlocal enableextensions
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..

set E2E_BIN=%SCRIPT_DIR%e2e_tests.exe

if exist "%E2E_BIN%" del "%E2E_BIN%" >nul 2>&1

gcc -std=c11 -O2 -Wall -Wextra -DUNIT_TEST -I"%ROOT_DIR%" ^
    -o "%E2E_BIN%" "%SCRIPT_DIR%e2e_tests.c" "%ROOT_DIR%\main.c"

pushd "%ROOT_DIR%" >nul
"%E2E_BIN%"
popd >nul
endlocal

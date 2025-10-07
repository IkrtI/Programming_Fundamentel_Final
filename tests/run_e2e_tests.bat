@echo off
setlocal enableextensions
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..

set E2E_BIN=%ROOT_DIR%\maint_tests.exe

if exist "%E2E_BIN%" del "%E2E_BIN%" >nul 2>&1

gcc -std=c11 -O2 -Wall -Wextra -DENABLE_INTERNAL_TESTS -I"%ROOT_DIR%" ^
    -o "%E2E_BIN%" "%ROOT_DIR%\main.c"

pushd "%ROOT_DIR%" >nul
"%E2E_BIN%" --run-e2e-tests
popd >nul
endlocal

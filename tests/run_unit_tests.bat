@echo off
setlocal enableextensions
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..

set UNIT_BIN=%ROOT_DIR%\maint_tests.exe

if exist "%UNIT_BIN%" del "%UNIT_BIN%" >nul 2>&1

gcc -std=c11 -O2 -Wall -Wextra -DENABLE_INTERNAL_TESTS -I"%ROOT_DIR%" ^
    -o "%UNIT_BIN%" "%ROOT_DIR%\main.c"

pushd "%ROOT_DIR%" >nul
"%UNIT_BIN%" --run-unit-tests
popd >nul
endlocal

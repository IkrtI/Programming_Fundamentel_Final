#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

E2E_BIN="$ROOT_DIR/maint_tests"

gcc -std=c11 -O2 -Wall -Wextra -fno-common -DENABLE_INTERNAL_TESTS -I"$ROOT_DIR" \
    -o "$E2E_BIN" "$ROOT_DIR/main.c"

cd "$ROOT_DIR"
"$E2E_BIN" --run-e2e-tests 2>&1

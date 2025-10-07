#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

E2E_BIN="$SCRIPT_DIR/e2e_tests"

gcc -std=c11 -O2 -Wall -Wextra -DUNIT_TEST -I"$ROOT_DIR" \
    -o "$E2E_BIN" "$SCRIPT_DIR/e2e_tests.c" "$ROOT_DIR/main.c"

cd "$ROOT_DIR"
"$E2E_BIN"

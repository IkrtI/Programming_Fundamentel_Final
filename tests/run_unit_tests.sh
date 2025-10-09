#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

UNIT_BIN="$ROOT_DIR/maint_tests"

gcc -std=c11 -O2 -Wall -Wextra -fno-common -DENABLE_INTERNAL_TESTS -I"$ROOT_DIR" \
    -o "$UNIT_BIN" "$ROOT_DIR/main.c"

cd "$ROOT_DIR"
"$UNIT_BIN" --run-unit-tests

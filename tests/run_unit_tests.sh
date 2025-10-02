#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

UNIT_BIN="$SCRIPT_DIR/unit_tests"

gcc -std=c11 -O2 -Wall -Wextra -DUNIT_TEST -I"$ROOT_DIR" \
    -o "$UNIT_BIN" "$SCRIPT_DIR/unit_tests.c" "$ROOT_DIR/main.c"

cd "$ROOT_DIR"
"$UNIT_BIN"

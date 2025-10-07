#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CC=${CC:-gcc}
CFLAGS="${CFLAGS:--std=c11 -Wall -Wextra -O2}"
EXTRA_DEFINES="-DENABLE_INTERNAL_TESTS"
OUTPUT="$SCRIPT_DIR/maint"
SRC="$SCRIPT_DIR/main.c"
INCLUDE="-I\"$SCRIPT_DIR\""

printf 'Building maint with %s %s %s\n' "$CC" "$CFLAGS" "$EXTRA_DEFINES"
"$CC" $CFLAGS $EXTRA_DEFINES $INCLUDE -o "$OUTPUT" "$SRC"
printf 'Output: %s\n' "$OUTPUT"

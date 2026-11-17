#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
binary=$(mktemp "${TMPDIR:-/tmp}/life-test.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -g \
    -fsanitize=address,undefined -Isrc \
    src/life.c src/app.c tests/test_life.c -o "$binary"
"$binary"

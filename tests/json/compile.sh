#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cc="${CC:-cc}"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT

"$cc" -std=c99 -Wall -Wextra -Werror -I"$root/include" \
    -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn" \
    "$root/tests/json/contract.c" \
    "$root/codecs/json/reader.c" \
    "$root/codecs/json/writer.c" \
    -o "$binary"

"$binary"

"$cc" -std=c99 -Wall -Wextra -Werror -I"$root/include" \
    -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn" \
    "$root/tests/json/link_collision.c" \
    "$root/codecs/json/reader.c" \
    "$root/codecs/json/writer.c" \
    -o "$binary"

"$binary"

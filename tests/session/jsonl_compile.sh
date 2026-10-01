#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror \
    -I"$root/include" -I"$root/src" \
    -I"$root/providers/storage/jsonl/include" \
    -I"$root/providers/storage/jsonl/src" \
    -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn" \
    "$root/tests/session/jsonl_contract.c" \
    "$root/providers/storage/jsonl/src/session_jsonl.c" \
    "$root/providers/storage/jsonl/src/jsonl_record.c" \
    "$root/codecs/json/reader.c" "$root/codecs/json/writer.c" \
    "$root/src/session/session_manager.c" "$root/src/core/arena.c" \
    -o "$binary"

"$binary"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
    -I"$root/include" -I"$root/providers/storage/jsonl/include" \
    -include agent_session_jsonl.h -x c++ -fsyntax-only /dev/null
printf 'PASS: JSONL filesystem Session provider\n'

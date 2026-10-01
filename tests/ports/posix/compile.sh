#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror \
    -I"$root/include" \
    -I"$root/providers/storage/jsonl/include" \
    -I"$root/providers/storage/jsonl/src" \
    -I"$root/ports/posix/storage/include" \
    -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn" \
    "$root/tests/ports/posix/storage_contract.c" \
    "$root/ports/posix/storage/src/session_files.c" \
    "$root/providers/storage/jsonl/src/session_jsonl.c" \
    "$root/providers/storage/jsonl/src/jsonl_record.c" \
    "$root/codecs/json/reader.c" "$root/codecs/json/writer.c" \
    -o "$binary"

"$binary"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
    -I"$root/include" -I"$root/providers/storage/jsonl/include" \
    -I"$root/ports/posix/storage/include" \
    -include agent_posix_session_files.h -x c++ -fsyntax-only /dev/null
printf 'PASS: POSIX JSONL Session files\n'

#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT
includes=(-I"$root/include" -I"$root/providers/storage/files/include"
  -I"$root/providers/storage/jsonl/include" -I"$root/providers/storage/jsonl/src"
  -I"$root/ports/posix/storage/include" -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn")
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} "${includes[@]}" \
  "$root/tests/ports/posix/file_store_contract.c" \
  "$root/providers/storage/files/src/file_store.c" \
  "$root/ports/posix/storage/src/file_store.c" \
  "$root/providers/storage/jsonl/src/file_store_bind.c" \
  "$root/providers/storage/jsonl/src/session_jsonl.c" \
  "$root/providers/storage/jsonl/src/jsonl_record.c" \
  "$root/codecs/json/reader.c" "$root/codecs/json/writer.c" -o "$binary"
"$binary"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror "${includes[@]}" \
  -include agent_posix_file_store.h -include agent_session_jsonl_files.h -x c++ -fsyntax-only /dev/null
printf 'PASS: POSIX file store, USER snapshot and JSONL Session bridge\n'

#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} \
  -I"$root/include" -I"$root/providers/storage/files/include" \
  "$root/tests/providers/files/contract.c" \
  "$root/providers/storage/files/src/file_store.c" -o "$binary"
"$binary"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -I"$root/include" -I"$root/providers/storage/files/include" \
  -include agent_file_store.h -x c++ -fsyntax-only /dev/null
printf 'PASS: bounded byte-file helpers\n'

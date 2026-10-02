#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
build_dir="${TMPDIR:-/tmp}/cagentv2-openvela-port-tests"
mkdir -p "$build_dir"

cc -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$root/include" \
  -I"$root/src" \
  -I"$root/ports/openvela/transport/include" \
  -I"$root/tests/ports/openvela/fake" \
  "$root/tests/ports/openvela/contract.c" \
  "$root/ports/openvela/transport/src/transport.c" \
  "$root/src/run/cancel.c" \
  -o "$build_dir/contract"

"$build_dir/contract"
printf '%s\n' 'PASS: OpenVela webclient Transport adapter contract (C99 mock SDK).'

cc -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$root/include" \
  -I"$root/ports/openvela/runtime/include" \
  "$root/tests/ports/openvela/runtime_contract.c" \
  "$root/ports/openvela/runtime/src/runtime.c" \
  -o "$build_dir/runtime_contract"

"$build_dir/runtime_contract"
printf '%s\n' 'PASS: OpenVela monotonic Runtime adapter contract (C99 mock clock).'

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic -DTEST_OPENVELA=1 -DTEST_JSONL=1 \
  -I"$root/include" -I"$root/providers/storage/files/include" -I"$root/providers/storage/jsonl/include" \
  -I"$root/ports/posix/storage/include" -I"$root/ports/openvela/storage/include" \
  "$root/tests/ports/file_store_binding.c" \
  "$root/providers/storage/files/src/file_store.c" \
  "$root/providers/storage/jsonl/src/file_store_bind.c" \
  "$root/ports/posix/storage/src/file_store.c" \
  "$root/ports/openvela/storage/src/file_store.c" -o "$build_dir/file_store"
"$build_dir/file_store"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -I"$root/include" -I"$root/providers/storage/files/include" \
  -I"$root/ports/posix/storage/include" -I"$root/ports/openvela/storage/include" \
  -include agent_openvela_file_store.h -x c++ -fsyntax-only /dev/null
printf '%s\n' 'PASS: OpenVela byte-file binding (Host filesystem contract).'

#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
build_dir="${TMPDIR:-/tmp}/cagentv2-espidf-port-tests"
mkdir -p "$build_dir"

for bundle in off on; do
  defines=()
  if [[ "$bundle" == on ]]; then
    defines=(-DCONFIG_MBEDTLS_CERTIFICATE_BUNDLE=1)
  fi
  cc -std=c99 -Wall -Wextra -Werror -pedantic "${defines[@]}" \
    -I"$root/include" \
    -I"$root/src" \
    -I"$root/ports/espidf/runtime/include" \
    -I"$root/ports/espidf/transport/include" \
    -I"$root/tests/ports/espidf/fake" \
    "$root/tests/ports/espidf/contract.c" \
    "$root/ports/espidf/runtime/src/runtime.c" \
    "$root/ports/espidf/transport/src/transport.c" \
    "$root/src/run/cancel.c" \
    -o "$build_dir/contract-$bundle"

  "$build_dir/contract-$bundle"
done
printf '%s\n' 'PASS: ESP-IDF Runtime and Transport adapter contract (C99 mock SDK).'

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic \
  -DTEST_ESPIDF=1 -DTEST_JSONL=1 -DAGENT_POSIX_FILE_STORE_NO_SYMLINKS=1 \
  -I"$root/include" -I"$root/providers/storage/files/include" -I"$root/providers/storage/jsonl/include" \
  -I"$root/ports/posix/storage/include" -I"$root/ports/espidf/storage/include" \
  "$root/tests/ports/file_store_binding.c" \
  "$root/providers/storage/files/src/file_store.c" \
  "$root/providers/storage/jsonl/src/file_store_bind.c" \
  "$root/ports/posix/storage/src/file_store.c" \
  "$root/ports/espidf/storage/src/file_store.c" -o "$build_dir/file_store"
"$build_dir/file_store"
if nm -u "$build_dir/file_store" | rg -q ' lstat'; then
  printf 'FAIL: ESP-IDF profile must not require lstat\n' >&2
  exit 1
fi
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -I"$root/include" -I"$root/providers/storage/files/include" \
  -I"$root/ports/posix/storage/include" -I"$root/ports/espidf/storage/include" \
  -include agent_espidf_file_store.h -x c++ -fsyntax-only /dev/null
printf '%s\n' 'PASS: ESP-IDF byte-file binding (Host no-symlink VFS profile).'

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

cc -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$root/include" \
  -I"$root/providers/storage/jsonl/include" \
  -I"$root/ports/posix/storage/include" \
  -I"$root/ports/espidf/storage/include" \
  "$root/tests/ports/espidf/storage_contract.c" \
  "$root/ports/espidf/storage/src/storage.c" \
  "$root/ports/posix/storage/src/session_files.c" \
  -o "$build_dir/storage_contract"

"$build_dir/storage_contract"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -I"$root/include" -I"$root/providers/storage/jsonl/include" \
  -I"$root/ports/posix/storage/include" \
  -I"$root/ports/espidf/storage/include" \
  -include agent_espidf_session_files.h -x c++ -fsyntax-only /dev/null
printf '%s\n' 'PASS: ESP-IDF Session Storage binding (Host VFS contract).'

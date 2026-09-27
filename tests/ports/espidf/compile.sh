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
    "$root/src/run/turn.c" \
    -o "$build_dir/contract-$bundle"

  "$build_dir/contract-$bundle"
done
printf '%s\n' 'PASS: ESP-IDF Runtime and Transport adapter contract (C99 mock SDK).'

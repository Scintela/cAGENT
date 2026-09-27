#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
build_dir="${TMPDIR:-/tmp}/cagentv2-espidf-port-tests"
mkdir -p "$build_dir"

cc -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$root/include" \
  -I"$root/ports/espidf/runtime/include" \
  -I"$root/ports/espidf/transport/include" \
  -I"$root/tests/ports/espidf/fake" \
  "$root/tests/ports/espidf/contract.c" \
  "$root/ports/espidf/runtime/src/runtime.c" \
  "$root/ports/espidf/transport/src/transport.c" \
  -o "$build_dir/contract"

"$build_dir/contract"
printf '%s\n' 'PASS: ESP-IDF Runtime and Transport adapter contract (C99 mock SDK).'

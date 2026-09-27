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
  "$root/src/run/turn.c" \
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

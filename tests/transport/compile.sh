#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="${TMPDIR:-/tmp}/cagentv2-transport-tests"
mkdir -p "$build_dir"

cc -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$root/include" -I"$root/src" \
  "$root/tests/transport/contract.c" \
  "$root/src/transport/transport.c" \
  -o "$build_dir/contract"

"$build_dir/contract"

#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
sources=("$root/tests/tool/contract.c" "$root/src/tool/tool_registry.c"
  "$root/src/tool/tool_schema.c" "$root/src/tool/tool_guard.c"
  "$root/src/policy/policy_chain.c" "$root/src/skill/skill_registry.c" "$root/src/context/context_registry.c" "$root/src/core/lifecycle.c"
  "$root/src/core/arena.c" "$root/src/model/model.c"
  "$root/src/run/cancel.c" "$root/src/runtime/runtime.c")
includes=(-I"$root/include" -I"$root/src" -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn")
for slots in 0 1 12; do
  extra=()
  if [[ "$slots" != 0 ]]; then extra=("$root/codecs/json/reader.c"); fi
  "${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} "${includes[@]}" \
    -DAGENT_MAX_TOOLS="$slots" "${sources[@]}" "${extra[@]}" -o "$build/tool-$slots"
  "$build/tool-$slots"
done
if "${CC:-cc}" -std=c99 "${includes[@]}" -DAGENT_CORE_WORKSPACE_BYTES=1024 \
    -c "$root/src/core/lifecycle.c" -o "$build/too-small.o" > "$build/small.log" 2>&1; then
  printf 'FAIL: insufficient workspace accepted\n' >&2
  exit 1
fi
rg -q agent_workspace_must_fit_core "$build/small.log"
printf 'PASS: zero/single/default Tool profiles and workspace compile-time rejection\n'

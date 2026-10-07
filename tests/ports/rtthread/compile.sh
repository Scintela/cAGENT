#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
build=$(mktemp -d "${TMPDIR:-/tmp}/cagent-rtthread-XXXXXX")
trap 'rm -rf "$build"' EXIT
flags=(-std=c99 -Wall -Wextra -Werror -pedantic -I"$root/include" -I"$root/src"
  -I"$root/tests/ports/rtthread/fake" -I"$root/ports/rtthread/runtime/include"
  -I"$root/ports/rtthread/transport/include")
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
for hz in 100 1000 1024; do
  for heap in off on; do
    defines=(-DRT_TICK_PER_SECOND="$hz")
    if [[ $heap == on ]]; then defines+=(-DRT_USING_HEAP); fi
    "${CC:-cc}" "${flags[@]}" "${defines[@]}" "$root/tests/ports/rtthread/runtime_contract.c" \
      "$root/ports/rtthread/runtime/src/runtime.c" -o "$build/runtime"
    "$build/runtime"
  done
done
if "${CC:-cc}" "${flags[@]}" -DWEBCLIENT_DEBUG -fsyntax-only \
    "$root/ports/rtthread/transport/src/transport.c" >"$build/debug.log" 2>&1; then
  printf '%s\n' 'FAIL: credential debug logging must be rejected' >&2; exit 1
fi
if "${CC:-cc}" "${flags[@]}" -DRTTHREAD_VERSION=50000 -fsyntax-only \
    "$root/ports/rtthread/runtime/src/runtime.c" >"$build/version.log" 2>&1; then
  printf '%s\n' 'FAIL: unsupported Runtime SDK version must be rejected' >&2; exit 1
fi
for tls in off on; do
  defines=()
  if [[ $tls == on ]]; then defines+=(-DWEBCLIENT_USING_SAL_TLS); fi
  "${CC:-cc}" "${flags[@]}" "${defines[@]}" "$root/tests/ports/rtthread/transport_contract.c" \
    "$root/ports/rtthread/transport/src/transport.c" "$root/src/transport/transport.c" \
    "$root/src/run/cancel.c" -o "$build/transport"
  "$build/transport"
done
for no_links in 0 1; do
  "${CC:-cc}" "${flags[@]}" -DTEST_RTTHREAD -DTEST_MEMORY -DTEST_JSONL \
    -DAGENT_POSIX_FILE_STORE_NO_SYMLINKS="$no_links" \
    -I"$root/providers/storage/files/include" -I"$root/providers/storage/jsonl/include" \
    -I"$root/providers/memory/markdown/include" -I"$root/ports/posix/storage/include" \
    -I"$root/ports/rtthread/storage/include" "$root/tests/ports/file_store_binding.c" \
    "$root/providers/storage/files/src/file_store.c" "$root/providers/storage/jsonl/src/file_store_bind.c" \
    "$root/providers/memory/markdown/src/memory_markdown.c" \
    "$root/ports/posix/storage/src/file_store.c" "$root/ports/rtthread/storage/src/file_store.c" -o "$build/files"
  "$build/files"
done
for header in agent_rtthread_runtime.h agent_rtthread_transport.h agent_rtthread_file_store.h; do
  "${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
    -I"$root/include" -I"$root/tests/ports/rtthread/fake" \
    -I"$root/ports/rtthread/runtime/include" -I"$root/ports/rtthread/transport/include" \
    -I"$root/ports/rtthread/storage/include" -I"$root/ports/posix/storage/include" \
    -I"$root/providers/storage/files/include" -include "$header" -x c++ -fsyntax-only /dev/null
done
printf '%s\n' 'PASS: RT-Thread Runtime (3 tick rates, heap on/off), POST Transport (TLS on/off), file binding (2 filesystem profiles), C++11 headers.'

#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror \
    -I"$root/include" -I"$root/src" \
    -I"$root/providers/storage/ram/include" \
    "$root/tests/session/contract.c" \
    "$root/src/session/session_manager.c" \
    "$root/src/session/session_storage.c" \
    "$root/providers/storage/ram/src/session_ram.c" \
    "$root/src/core/lifecycle.c" \
    "$root/src/core/arena.c" \
    "$root/src/model/model.c" \
    "$root/src/runtime/runtime.c" \
    -o "$binary"

"$binary"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
    -I"$root/include" -I"$root/providers/storage/ram/include" \
    -include agent_session_ram.h -x c++ -fsyntax-only /dev/null
printf 'PASS: Session transaction, projection and volatile Storage\n'

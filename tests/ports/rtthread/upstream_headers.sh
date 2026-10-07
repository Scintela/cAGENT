#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
: "${RTTHREAD_SDK_ROOT:?Set to a local RT-Thread checkout}"
: "${WEBCLIENT_SDK_ROOT:?Set to a local RT-Thread WebClient checkout}"
includes=(-I"$root/include" -I"$root/src" -I"$root/tests/ports/rtthread/sdk"
  -I"$root/ports/rtthread/runtime/include" -I"$root/ports/rtthread/transport/include"
  -isystem "$RTTHREAD_SDK_ROOT/include" -isystem "$WEBCLIENT_SDK_ROOT/inc")
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${includes[@]}" -fsyntax-only \
  "$root/ports/rtthread/runtime/src/runtime.c" "$root/ports/rtthread/transport/src/transport.c"
printf '%s\n' 'PASS: Runtime/Transport compile against real upstream headers (minimal UP profile, no firmware link or TLS verification).'

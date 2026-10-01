#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cc="${CC:-cc}"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT

"$cc" -std=c99 -Wall -Wextra -Werror \
    -I"$root/include" -I"$root/src" \
    -I"$root/providers/model/openai/include" -I"$root/providers/model/openai/src" \
    -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn" \
    "$root/tests/providers/openai/contract.c" \
    "$root/providers/model/openai/src/model_openai.c" \
    "$root/providers/model/openai/src/openai_request.c" \
    "$root/providers/model/openai/src/openai_response.c" \
    "$root/codecs/json/reader.c" \
    "$root/codecs/json/writer.c" \
    "$root/src/core/lifecycle.c" \
    "$root/src/core/arena.c" \
    "$root/src/model/model.c" \
    "$root/src/run/cancel.c" \
    "$root/src/runtime/runtime.c" \
    -o "$binary"

"$binary"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
    -I"$root/include" -I"$root/providers/model/openai/include" \
    -include agent_openai_model.h -x c++ -fsyntax-only /dev/null

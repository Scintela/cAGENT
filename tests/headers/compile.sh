#!/usr/bin/env bash
# Compile-only contract checks; these do not validate runtime behavior.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd -- "$root"

cc=${CC:-cc}
cxx=${CXX:-c++}
flags=(-Wall -Wextra -Werror -pedantic -Iinclude -Isrc -fsyntax-only)
mapfile -t headers < <(rg --files include src -g '*.h' | LC_ALL=C sort)

emit_header() {
    local path=$1
    path=${path#include/}
    path=${path#src/}
    printf '#include <%s>\n#include <%s>\n' "$path" "$path"
}

for header in "${headers[@]}"; do
    {
        emit_header "$header"
        printf 'int main(void) { return 0; }\n'
    } | "$cc" -std=c99 -x c "${flags[@]}" -
    {
        emit_header "$header"
        printf 'int main() { return 0; }\n'
    } | "$cxx" -std=c++11 -x c++ "${flags[@]}" -
done

# Test all headers together in both orders, catching typedef/guard collisions.
for order in forward reverse; do
    for language in c cpp; do
        {
            if [[ $order == forward ]]; then
                for header in "${headers[@]}"; do emit_header "$header"; done
            else
                for ((i=${#headers[@]}-1; i>=0; --i)); do
                    emit_header "${headers[i]}"
                done
            fi
            printf 'int main(void) { return 0; }\n'
        } | if [[ $language == c ]]; then
            "$cc" -std=c99 -x c "${flags[@]}" -
        else
            "$cxx" -std=c++11 -x c++ "${flags[@]}" -
        fi
    done
done

"$cc" -std=c99 -x c "${flags[@]}" tests/headers/contracts.c
"$cxx" -std=c++11 -x c++ "${flags[@]}" tests/headers/contracts.c
printf 'PASS: %d headers, standalone/repeated/combined C99 and C++11 (%s, %s).\n' \
    "${#headers[@]}" "$cc" "$cxx"

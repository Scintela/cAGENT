#!/bin/sh
# cAgentV2 红线检验（与 rules/engineering.md 同址维护）
# 用法：在仓库任意位置运行 bash rules/checks.sh；退出码 0 即全部通过。
# 覆盖：E1 动态内存 / E2 系统头白名单 / E3 预处理边界 / E5 可重入。
set -u
cd "$(dirname "$0")/.." || exit 2

status=0

report() {
    if [ -n "$2" ]; then
        printf 'FAIL %s\n%s\n' "$1" "$2"
        status=1
    else
        printf 'PASS %s\n' "$1"
    fi
}

e1=$(grep -rnE '(^|[^.>[:alnum:]_])(malloc|calloc|realloc|free)[[:space:]]*\(' \
    src include --include='*.c' --include='*.h' | sed 's/^/  /' || true)
report 'E1 动态内存' "$e1"

e2=$(grep -rh '#include <' src include --include='*.c' --include='*.h' \
    | sed -e 's/^[[:space:]]*//' -e 's/^#include[[:space:]]*<\(.*\)>.*/\1/' \
    | sort -u \
    | grep -vE '^(agent\.h$|agent/|string\.h$|stdint\.h$|stddef\.h$|stdbool\.h$)' \
    | sed 's/^/  /' || true)
report 'E2 系统头白名单' "$e2"

e3=$(grep -rn '#if' src --include='*.c' --include='*.h' \
    | grep -v '__cplusplus' | grep -v '#if AGENT_' | sed 's/^/  /' || true)
report 'E3 预处理边界' "$e3"

e5=$(grep -rnE '^static |^[[:space:]]+static ' src include --include='*.c' --include='*.h' \
    | grep -v 'static const' | grep -v '(' | sed 's/^/  /' || true)
report 'E5 可重入' "$e5"

exit "$status"

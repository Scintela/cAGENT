#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
# 一键回归：运行 tests/ 下全部自包含测试脚本。需要本地目标源码树的
# 检查（*cross_compile* 需 NUTTX_ROOT、upstream_headers 需 RT-Thread SDK）
# 除外；CC 与 CFLAGS 透传给各脚本。
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
status=0

while IFS= read -r script; do
    printf '==> %s\n' "${script#"$root"/}"
    if ! bash "$script"; then
        printf 'FAIL: %s\n' "${script#"$root"/}"
        status=1
    fi
done < <(find "$root/tests" -name '*.sh' ! -name 'run_all.sh' \
         ! -name '*cross_compile*' ! -name 'upstream_headers.sh' | sort)

if [ "$status" -ne 0 ]; then
    printf 'run_all: FAILED\n'
    exit 1
fi
printf 'run_all: all suites passed\n'

#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
build=$(mktemp -d "${TMPDIR:-/tmp}/cagent-rtthread-build-XXXXXX")
trap 'rm -rf "$build"' EXIT
for profile in off runtime transport files all; do
  args=()
  if [[ $profile == runtime || $profile == all ]]; then args+=(-DAGENT_RTTHREAD_RUNTIME=ON); fi
  if [[ $profile == transport || $profile == all ]]; then args+=(-DAGENT_RTTHREAD_TRANSPORT=ON); fi
  if [[ $profile == files || $profile == all ]]; then
    args+=(-DAGENT_RTTHREAD_FILE_STORE=ON -DAGENT_BUILD_FILE_STORE=ON -DAGENT_BUILD_POSIX_FILE_STORE=ON)
  fi
  cmake -S "$root/tests/ports/rtthread/build" -B "$build/$profile" "${args[@]}" >/dev/null
  cmake --build "$build/$profile" -j2 >/dev/null
  for test in runtime_contract transport_contract file_contract; do
    if [[ -x $build/$profile/$test ]]; then "$build/$profile/$test"; fi
  done
done
python3 "$root/tests/ports/rtthread/scons_contract.py"
printf '%s\n' 'PASS: RT-Thread CMake target selection/linking and SCons source/profile contract.'
if [[ -n ${SCONS:-} ]]; then
  "$SCONS" -Q -f "$root/tests/ports/rtthread/scons/SConstruct" \
    ROOT_DIR="$root" BUILD_DIR="$build/scons" -j2 >/dev/null
  "$build/scons/runtime_contract"
  "$build/scons/transport_contract"
  printf '%s\n' 'PASS: real SCons compilation/linking with Host SDK doubles and shared rtconfig.h.'
fi
if [[ -n ${KCONFIG_PYTHON:-} ]]; then
  "$KCONFIG_PYTHON" "$root/tests/ports/rtthread/kconfig_contract.py"
fi

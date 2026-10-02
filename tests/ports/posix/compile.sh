#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail

directory="$(cd "$(dirname "$0")" && pwd)"
bash "$directory/file_store_compile.sh"
bash "$directory/file_store_faults.sh"

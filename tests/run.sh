#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "$0")/.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf -- "$test_dir"' EXIT
compiler="${CXX:-g++}"
flags=(-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
"$compiler" "${flags[@]}" -I"$project_dir/include" "$project_dir/tests/format_test.cpp" -o "$test_dir/format-test"
"$test_dir/format-test"
"$compiler" "${flags[@]}" -I"$project_dir/tests/platform" -I"$project_dir/include" "$project_dir/tests/restore_test.cpp" "$project_dir/source/save.cpp" -Wl,--wrap=rename -o "$test_dir/restore-test"
cd -- "$test_dir"
./restore-test
"$compiler" "${flags[@]}" -I"$project_dir/tests/platform" -I"$project_dir/include" "$project_dir/tests/ui_test.cpp" "$project_dir/source/ui.cpp" -o "$test_dir/ui-test"
"$test_dir/ui-test" "$project_dir/tests/fixtures/icon.smdh"
"$compiler" "${flags[@]}" -I"$project_dir/tests/platform" -I"$project_dir/include" "$project_dir/tests/presentation_test.cpp" "$project_dir/source/operation.cpp" "$project_dir/source/ui.cpp" -o "$test_dir/presentation-test"
"$test_dir/presentation-test"

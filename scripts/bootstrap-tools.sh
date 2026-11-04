#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Download pinned, public homebrew build tools into this checkout only.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "$0")/.." && pwd)"
if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  echo 'This helper supports Linux x86_64 (including WSL2). See README for other systems.' >&2
  exit 1
fi
for command_name in curl unzip sha256sum; do command -v "$command_name" >/dev/null; done
tool_dir="$project_dir/.tools/bin"
temp_dir="$(mktemp -d)"
trap 'rm -rf -- "$temp_dir"' EXIT
mkdir -p "$tool_dir"
curl --fail --location --retry 3 --silent --show-error \
  https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.18.3/makerom-v0.18.3-ubuntu_x86_64.zip \
  -o "$temp_dir/makerom.zip"
curl --fail --location --retry 3 --silent --show-error \
  https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip \
  -o "$temp_dir/bannertool.zip"
printf '%s  %s\n' \
  6f36f77946742d2b8286a92d887e845e06d430998ed4a9c4856d8ea2d4eebd6f "$temp_dir/makerom.zip" \
  e4259c08fe8944ebadd5f4b96f9a8603e5427338074cfc46323fbbc3410d51ed "$temp_dir/bannertool.zip" | sha256sum --check
unzip -p "$temp_dir/makerom.zip" makerom > "$tool_dir/makerom"
unzip -p "$temp_dir/bannertool.zip" linux-x86_64/bannertool > "$tool_dir/bannertool"
chmod +x "$tool_dir/makerom" "$tool_dir/bannertool"
printf 'Tools ready: %s\n' "$tool_dir"

#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

echo "CODEX: starting in ${repo_root}"
echo "CODEX: approval policy = never"
echo "CODEX: sandbox = workspace-write"

exec codex \
  --cd "${repo_root}" \
  --ask-for-approval never \
  --sandbox workspace-write \
  "$@"

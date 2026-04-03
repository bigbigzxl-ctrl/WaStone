#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

echo "CODEX: starting in ${repo_root}"
echo "CODEX: approval policy = on-request"
echo "CODEX: sandbox = danger-full-access"

exec codex \
  --cd "${repo_root}" \
  --ask-for-approval on-request \
  --sandbox danger-full-access \
  "$@"

 #!/usr/bin/env bash
  set -euo pipefail

  echo "== basic =="
  echo "pwd: $(pwd)"
  echo "user: $(id -un)"
  echo "shell: ${SHELL:-unknown}"

  echo
  echo "== codex binary =="
  command -v codex || true
  type codex || true
  readlink -f "$(command -v codex)" 2>/dev/null || true

  echo
  echo "== codex help =="
  codex --help 2>&1 | sed -n '1,220p' || true

  echo
  echo "== codex env =="
  env | sort | grep -i codex || true

  echo
  echo "== sandbox/approval env =="
  env | sort | grep -E 'SANDBOX|APPROVAL|BWRAP|BUBBLEWRAP|CODEX' || true

  echo
  echo "== parent process chain =="
  ps -o pid=,ppid=,comm=,args= -p $$ || true
  ppid="$(ps -o ppid= -p $$ | tr -d ' ')"
  while [ -n "${ppid}" ] && [ "${ppid}" != "0" ]; do
    ps -o pid=,ppid=,comm=,args= -p "${ppid}" || true
    next_ppid="$(ps -o ppid= -p "${ppid}" | tr -d ' ' || true)"
    [ -z "${next_ppid}" ] && break
    [ "${next_ppid}" = "${ppid}" ] && break
    ppid="${next_ppid}"
  done

  echo
  echo "== bwrap/unshare =="
  command -v bwrap || true
  bwrap --version 2>&1 || true
  command -v unshare || true
  unshare --version 2>&1 || true

  echo
  echo "== kernel settings =="
  sysctl kernel.unprivileged_userns_clone 2>/dev/null || true
  sysctl user.max_user_namespaces 2>/dev/null || true

  echo
  echo "== smoke tests =="
  echo "-- plain shell --"
  sh -c 'echo shell_ok' 2>&1 || true

  echo "-- unshare --"
  unshare -Ur true 2>&1 || true

  echo "-- bwrap --"
  bwrap --ro-bind / / --proc /proc --dev /dev sh -c 'echo bwrap_ok' 2>&1 || true


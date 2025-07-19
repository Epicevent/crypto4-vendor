#!/usr/bin/env bash
set -euo pipefail

# Git Bash/MSYS2용 cygpath 체크: WSL이라면 그냥 PWD 사용
if command -v cygpath &>/dev/null; then
  ROOT="$(cygpath -u "${PWD}")"
else
  ROOT="${PWD}"
fi

cd "$ROOT"

echo "==> Initializing submodules…"
git submodule update --init --recursive

echo "==> Cleaning…"
make clean

echo "==> Building M4RI submodule…"
make m4ri

echo "==> Building core & tools…"
make all

echo "==> Build complete. Executables are in bin/"

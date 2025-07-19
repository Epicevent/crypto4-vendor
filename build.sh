#!/usr/bin/env bash
set -euo pipefail

# 1) 현재 디렉터리가 프로젝트 루트인지 확인
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# 2) 서브모듈 초기화 (최초 1회)
echo "==> Initializing submodules…"
git submodule update --init --recursive

# 3) M4RI 빌드 (최초 1회)
read -p "Do you want to build M4RI now? (y/n): " answer
if [[ "$answer" =~ ^[Yy]$ ]]; then
    echo "==> Building M4RI…"
    make m4ri
else
    echo "Skipping M4RI build."
fi
echo "M4RI is built."
# 4) core 빌드
echo "==> Building core…"
make core
bin/debug_init
# 5) tools 빌드
echo "==> Building tools…"
make tools
bin/gen_zS_bin
bin/gen_s_gt_bin_no_header
# 6) 테스트 실행
echo "==> Running tests…"
bin/simple_test

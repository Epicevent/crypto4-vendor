#!/usr/bin/env bash
set -euo pipefail

# 1) 프로젝트 루트로 이동
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

# 4) simple_test 빌드 및 실행
echo "==> Building simple_test…"
make simple_test
echo "-> Running simple_test…"
bin/simple_test


# 5) tools 빌드 및 실행
echo "==> Building tools…"
make tools
echo "-> Running gen_zS_bin…"
bin/gen_zS_bin
echo "-> Running gen_s_gt_bin…"
bin/gen_s_gt_bin
echo "-> Running gen_r4_patterns…"
bin/gen_r4_patterns
echo "-> Running verify_r4_pattern_rule…"
bin/verify_r4_pattern_rule

# 6) H.bin 생성
echo "==> Generating H.bin…"
bin/gen_H_bin

# 7) encrypt_test 빌드 및 실행
echo "==> Building encrypt_test…"
make encrypt_test
echo "-> Running encrypt_test…"
bin/encrypt_test

# 8) decrypt_test 실행 (맨 마지막)
echo "-> building decrypt_test…"
make decrypt_test
echo "-> Running decrypt_tool…"
bin/decrypt_test

echo "All builds and tests completed successfully."

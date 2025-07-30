````markdown
# Quick Start (WSL/Ubuntu Only)

이 프로젝트는 **WSL(Ubuntu)** 환경에서만 빌드 & 검증됩니다. 아래 절차대로 `build.sh` 하나만 실행하세요.

---

## 1. 필수 도구 설치 (최초 1회)

```bash
sudo apt update
sudo apt install -y build-essential autoconf automake libtool pkgconf git
````

## 2. 리포지토리 클론 & 초기화 (최초 1회)

```bash
git clone git@github.com:epicevent/crypto4-vendor.git
cd crypto4-vendor
git submodule update --init --recursive
chmod +x build.sh
```

## 3. `build.sh` 실행 & 확인

```bash
./build.sh
```

성공하면 마지막에 다음 메시지가 출력됩니다:

```
All builds and tests completed successfully.
```

이 메시지가 보이면 **빌드와 기본 동작 검증이 완료된 것**입니다. 이후부터는 `make clean all` 으로 core 실행파일만 빠르게 다시 빌드하거나, `bin/debug_init` / `bin/simple_test` 를 직접 실행하시면 됩니다.

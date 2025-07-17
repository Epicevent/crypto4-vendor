FROM ubuntu:22.04

# 필수 도구
RUN apt-get update && apt-get install -y curl xz-utils make gcc g++ git

# MSYS2 설치
RUN curl -L https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64.tar.xz \
     | tar -xJ -C /msys2

# PATH에 MSYS2 경로 추가
ENV PATH="/msys2/usr/bin:/msys2/mingw64/bin:${PATH}"

# MSYS2 업데이트 & 툴체인·M4RI 설치
RUN bash -lc "pacman -Sy --noconfirm \
      mingw-w64-x86_64-toolchain \
      mingw-w64-x86_64-m4ri"

WORKDIR /workspace

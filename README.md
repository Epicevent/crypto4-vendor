
## 개발 환경 설정

1. MSYS2 설치 ([https://www.msys2.org/](https://www.msys2.org/)) 후 쉘 실행

2. **MSYS2 MSYS/UCRT64 셸**에서

   ```bash
   pacman -Syu
   pacman -Su
   pacman -S --needed base-devel autoconf automake libtool pkgconf
   ```

3. M4RI 소스 클론 및 `configure` 생성

   ```bash
   cd ~/build
   git clone https://github.com/malb/m4ri.git
   cd m4ri
   autoreconf -fi
   ```

4. **MSYS2 MinGW64 셸**에서

   ```bash
   pacman -Syu
   pacman -Su
   pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake
   cd ~/build/m4ri
   mkdir -p build && cd build
   ../configure --host=x86_64-w64-mingw32 --prefix=/mingw64
   mingw32-make -j$(nproc)
   mingw32-make install
   ```

5. 설치 확인 및 프로젝트 빌드

   ```bash
   ls /mingw64/lib/libm4ri.a
   ls /mingw64/include/m4ri/m4ri.h
   cd ~/Documents/workspace/crypto4
   mingw32-make clean
   mingw32-make
   ```


## Windows 환경 변수 등록

1. **윈도우 검색**에 “환경 변수 편집” 입력 후 실행
2. **환경 변수(N)…** 클릭
3. **Path** 편집 → **새로 만들기**
4. 다음 경로 추가:

   ```
   C:\msys64\mingw64\bin
   ```
5. **확인** 후 모든 창 닫기

이제 PowerShell/CMD에서도 `gcc`, `make`, `m4ri` 헤더·라이브러리를 바로 사용할 수 있습니다.




#!/usr/bin/env bash
# Ubuntu 24.04: clang-18 lld-18 wasi-libc libc++-18-dev-wasm32 libclang-rt-18-dev-wasm32
set -eu
cd "$(dirname "$0")"
sdk=${WASI_SYSROOT:-/usr}
clang=${WASI_CXX:-clang++-18}
lib="$sdk/lib/llvm-18"
"$clang" --target=wasm32-wasi --sysroot="$sdk" -std=c++20 -Oz -fno-exceptions \
  -isystem "$lib/include/wasm32-wasi/c++/v1" -B"$sdk/bin" \
  -resource-dir "$lib/lib/clang/18" -isystem "$("$clang" -print-resource-dir)/include" -mexec-model=reactor \
  stage.cpp browser_stage.cpp -Wl,--strip-all -o ../frontend/src/stage.wasm
chmod 644 ../frontend/src/stage.wasm

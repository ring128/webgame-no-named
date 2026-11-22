#!/usr/bin/env bash
# Run from any directory: bash play.sh
set -eu
cd "$(dirname "$0")"
if ! node -e 'process.exit(Number(process.versions.node.split(".")[0])>=22?0:1)' 2>/dev/null; then
  if [[ -s "${NVM_DIR:-$HOME/.nvm}/nvm.sh" ]]; then
    . "${NVM_DIR:-$HOME/.nvm}/nvm.sh"
    nvm use 22 >/dev/null
  fi
fi
if [[ ! -x frontend/node_modules/.bin/vite ]]; then npm --prefix frontend ci; fi
npm --prefix frontend run build
dep="$PWD/backend/build/packages/prefix/usr"
args=()
if [[ -d "$dep/include/drogon" ]]; then
  lib="$dep/lib/$(gcc -dumpmachine)"
  export LD_LIBRARY_PATH="$lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  args+=("-DCMAKE_PREFIX_PATH=$dep" "-DMYSQL_INCLUDE_DIRS=$dep/include/mariadb"
         "-DMYSQL_LIBRARIES=$lib/libmariadbclient.so" "-DCMAKE_EXE_LINKER_FLAGS=-L$lib -Wl,-rpath-link,$lib")
fi
cmake -S backend -B backend/build/local "${args[@]}" -Wno-dev
cmake --build backend/build/local -j2
echo "ブラウザで http://localhost:8848/ を開いてください。終了は Ctrl+C"
exec backend/build/local/game_server frontend/dist

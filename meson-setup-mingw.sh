#!/bin/sh
# Configure MinGW cross-build (static libgcc/libstdc++/winpthread via mingw-static-pthread.specs).
set -e
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
BUILD="${1:-$ROOT/build-mingw}"
shift || true
exec meson setup "$BUILD" \
  --cross-file "$ROOT/src/wwlib/cross_mingw32.txt" \
  -Dplatform=windows \
  "$@"

#!/bin/bash
# Compiles shader.c/cpp → .wasm
#
# Prefers clang + wasm-ld (preserves WASM imports for WGSL built-in mapping).
# Falls back to emcc (which resolves math imports from its own libc).
#
# Requires one of:
#   brew install llvm
#   brew install emscripten
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="${1:-$SCRIPT_DIR/examples/shader.cpp}"
OUT="${SRC%.*}.wasm"

# ---------- try clang + wasm-ld (Homebrew LLVM or emsdk upstream) ----------
LLVM_BIN=""
for dir in \
  /opt/homebrew/opt/llvm/bin \
  /usr/local/opt/llvm/bin \
  "$HOME"/emsdk/upstream/bin \
; do
  if [ -x "$dir/clang" ] && [ -x "$dir/wasm-ld" ]; then
    LLVM_BIN="$dir"
    break
  fi
done

if [ -n "$LLVM_BIN" ]; then
  echo "Using LLVM from $LLVM_BIN"
  OBJ="${SRC%.*}.o"
  CLANG="$LLVM_BIN/clang"
  case "$SRC" in *.cpp|*.cc|*.cxx) CLANG="$LLVM_BIN/clang++" ;; esac
  "$CLANG" --target=wasm32 -O2 -c -fno-exceptions -fno-rtti -I"$SCRIPT_DIR" "$SRC" -o "$OBJ"
  "$LLVM_BIN/wasm-ld" \
    --no-entry \
    --allow-undefined \
    --export=mainImage \
    --export=memory \
    --initial-memory=1048576 \
    -o "$OUT" \
    "$OBJ"
  rm -f "$OBJ"
  echo "Built $OUT ($(wc -c < "$OUT" | tr -d ' ') bytes)"
  exit 0
fi

# ---------- fallback: emcc (Emscripten) ----------
EMCC=""
if command -v emcc &>/dev/null; then
  EMCC="emcc"
else
  for candidate in \
    "$HOME"/emsdk/upstream/emscripten/emcc \
  ; do
    if [ -x "$candidate" ]; then
      EMCC="$candidate"
      break
    fi
  done
fi

if [ -n "$EMCC" ]; then
  echo "Using emcc: $EMCC (note: math imports will be resolved, not preserved)"
  "$EMCC" -O2 \
    --no-entry \
    -s EXPORTED_FUNCTIONS='["_mainImage"]' \
    -s STANDALONE_WASM \
    -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
    -I"$SCRIPT_DIR" \
    -o "$OUT" \
    "$SRC"
  echo "Built $OUT ($(wc -c < "$OUT" | tr -d ' ') bytes)"
  exit 0
fi

# ---------- neither found ----------
echo "Error: no WASM toolchain found."
echo ""
echo "Install one of:"
echo "  brew install llvm"
echo "  brew install emscripten"
exit 1

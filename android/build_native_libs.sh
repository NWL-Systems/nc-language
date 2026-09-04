#!/bin/bash
# Cross-compila nclang, nc-cmd e nc_setup pro Android usando o NDK.
# Gera "libnclang.so" etc (sao executaveis normais, so tem nome de .so pra
# o Android aceitar empacotar e RODAR - desde o Android 10 o sistema so
# deixa executar binarios que estao na pasta nativeLibraryDir do app, e so
# arquivos terminados em .so entram la).
# © 2026 NWL-Systems
set -e

NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-$ANDROID_NDK}}"
if [ -z "$NDK" ]; then
    echo "✗ Defina ANDROID_NDK_HOME (ou ANDROID_NDK_ROOT) apontando pro NDK instalado."
    exit 1
fi

API=24   # Android 7.0+ (cobre praticamente todo aparelho em uso)
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/android/libs"

# Descobre a pasta de toolchain do NDK (varia por versao/SO do host)
if [ -d "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin" ]; then
    TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
elif [ -d "$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin" ]; then
    TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin"
else
    echo "✗ Nao encontrei toolchains/llvm/prebuilt/*/bin dentro de $NDK"
    exit 1
fi

build_abi() {
    local abi=$1 cc=$2
    mkdir -p "$OUT/$abi"
    "$TOOLCHAIN/$cc" -O2 -o "$OUT/$abi/libnclang.so"  "$ROOT/compiler/nc_compiler.c" -lm
    "$TOOLCHAIN/$cc" -O2 -o "$OUT/$abi/libnccmd.so"   "$ROOT/compiler/nc-cmd.c"
    "$TOOLCHAIN/$cc" -O2 -o "$OUT/$abi/libncsetup.so" "$ROOT/extensions/fs/nc_setup.c" "$ROOT/extensions/fs/nc_fs.c"
    echo "✓ $abi -> $OUT/$abi/"
}

echo "=== Compilando binarios nativos pro Android (NDK API $API) ==="
build_abi arm64-v8a   "aarch64-linux-android${API}-clang"
build_abi armeabi-v7a "armv7a-linux-androideabi${API}-clang"
echo "=== Pronto ==="

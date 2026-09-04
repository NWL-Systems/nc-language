#!/bin/bash
# Build NC Language + NuclearCloud OS Extensions - todas as plataformas
# © 2026 NWL-Systems
set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT/bin"
mkdir -p "$OUT"

echo "=== NC Language / NuclearCloud OS - Build ==="
echo "© 2026 NWL-Systems"
echo ""

# Detecta o compilador C disponivel: clang no Termux/Mac, gcc no Linux comum.
CC_BIN="${CC:-}"
if [ -z "$CC_BIN" ]; then
    if command -v clang >/dev/null 2>&1; then CC_BIN=clang
    elif command -v cc >/dev/null 2>&1; then CC_BIN=cc
    elif command -v gcc >/dev/null 2>&1; then CC_BIN=gcc
    else
        echo "✗ Nenhum compilador C encontrado (clang/gcc/cc)."
        echo "  Termux:  pkg install clang"
        echo "  Debian:  sudo apt install gcc"
        exit 1
    fi
fi
echo "Usando compilador: $CC_BIN"
echo ""

# 1) nclang - o compilador NC Language
echo "-- nclang (compilador NC Language) --"
"$CC_BIN" -O2 -o "$OUT/nclang" "$ROOT/compiler/nc_compiler.c" -lm
ln -sf nclang "$OUT/nuclearcloud"
echo "✓ $OUT/nclang (+ symlink nuclearcloud)"

# 2) nc-cmd - shell interativo NC
echo "-- nc-cmd (shell interativo) --"
"$CC_BIN" -O2 -o "$OUT/nc-cmd" "$ROOT/compiler/nc-cmd.c"
echo "✓ $OUT/nc-cmd"

# 3) nc_setup + libfs - inicializador da estrutura de pastas do NuclearCloud OS
echo "-- nc_setup (estrutura de pastas do NuclearCloud OS) --"
"$CC_BIN" -O2 -o "$OUT/nc_setup" "$ROOT/extensions/fs/nc_setup.c" "$ROOT/extensions/fs/nc_fs.c"
echo "✓ $OUT/nc_setup"

# Windows via MinGW. gcc-mingw32 (Debian/Ubuntu) usa prefixo -gcc; o pacote
# do Termux e' o llvm-mingw-w64, que usa prefixo -clang.
WIN_CC=""
if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    WIN_CC=x86_64-w64-mingw32-gcc
elif command -v x86_64-w64-mingw32-clang >/dev/null 2>&1; then
    WIN_CC=x86_64-w64-mingw32-clang
fi

if [ -n "$WIN_CC" ]; then
    echo "-- Windows (MinGW: $WIN_CC) --"
    "$WIN_CC" -O2 -o "$OUT/nclang.exe" "$ROOT/compiler/nc_compiler.c" -lm && echo "✓ $OUT/nclang.exe"
    "$WIN_CC" -O2 -o "$OUT/nc-cmd.exe" "$ROOT/compiler/nc-cmd.c" && echo "✓ $OUT/nc-cmd.exe"
    "$WIN_CC" -O2 -o "$OUT/nc_setup.exe" "$ROOT/extensions/fs/nc_setup.c" "$ROOT/extensions/fs/nc_fs.c" && echo "✓ $OUT/nc_setup.exe"
else
    echo ""
    echo "⚠ MinGW nao encontrado - build Windows pulado."
    echo "  Termux:         pkg install llvm-mingw-w64"
    echo "  Debian/Ubuntu:  sudo apt install mingw-w64"
fi

echo ""
echo "=== Build completo! ==="
echo "Binarios em: $OUT"
echo "Uso: $OUT/nclang programa.nc"

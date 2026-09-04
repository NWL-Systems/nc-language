#!/bin/bash
# Atualiza os arquivos locais do NC Language / NuclearCloud OS e reconstroi
# os binarios. Roda dentro da pasta do projeto (clonada via git).
# © 2026 NWL-Systems
set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

echo "=== NC Language / NuclearCloud OS - Update ==="
echo "© 2026 NWL-Systems"
echo ""

if [ ! -d .git ]; then
    echo "✗ Esta pasta nao e um clone git (nao tem .git/)."
    echo ""
    echo "Se voce baixou o projeto como .zip, o update automatico nao"
    echo "funciona - baixe a versao mais nova manualmente em:"
    echo "  https://github.com/NWL-Systems/nuclearcloud-nc-language"
    echo ""
    echo "Se quiser habilitar update automatico daqui pra frente, clone"
    echo "com git em vez de baixar o zip:"
    echo "  git clone https://github.com/NWL-Systems/nuclearcloud-nc-language.git"
    exit 1
fi

# Se o usuario mexeu em algo local, avisa em vez de sobrescrever sem querer
if [ -n "$(git status --porcelain)" ]; then
    echo "⚠ Voce tem alteracoes locais nao commitadas:"
    git status --porcelain
    echo ""
    read -p "Continuar mesmo assim e sobrescrever com git pull? [s/N] " resp
    if [ "$resp" != "s" ] && [ "$resp" != "S" ]; then
        echo "Update cancelado. Faca commit ou stash das suas alteracoes primeiro."
        exit 1
    fi
fi

echo "-- Baixando atualizacoes --"
git fetch origin
BEFORE=$(git rev-parse HEAD)
git pull --ff-only origin "$(git rev-parse --abbrev-ref HEAD)" || {
    echo ""
    echo "✗ Nao foi possivel atualizar automaticamente (historico divergente)."
    echo "  Resolva manualmente com 'git pull' ou 'git merge'."
    exit 1
}
AFTER=$(git rev-parse HEAD)

if [ "$BEFORE" = "$AFTER" ]; then
    echo "✓ Ja esta na versao mais recente."
else
    echo "✓ Atualizado: $BEFORE -> $AFTER"
    echo ""
    echo "-- Reconstruindo binarios --"
    ./build_all.sh
fi

echo ""
echo "=== Update completo! ==="

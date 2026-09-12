#!/bin/bash
# Публикация сайта на ветке gh-pages (GitHub Pages).
# Запуск из корня репозитория: website/deploy.sh
# Ветка полностью заменяется текущим содержимым website/ — она генерируемая.

set -euo pipefail
cd "$(dirname "$0")/.."

REV=$(git rev-parse --short HEAD)
NAME=$(git config user.name)
EMAIL=$(git config user.email)

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cp -r website/. "$TMP"/
rm -f "$TMP/deploy.sh" "$TMP/img/mandel.txt"

git -C "$TMP" init -q
git -C "$TMP" checkout -q -b gh-pages
git -C "$TMP" add -A
git -C "$TMP" -c user.name="$NAME" -c user.email="$EMAIL" \
    commit -qm "site: QTIV из коммита $REV"
git -C "$TMP" push -q --force https://github.com/I114rk/QTIV.git gh-pages:gh-pages
echo "gh-pages обновлён из $REV"

#!/bin/bash
# Сборка .deb-пакета из обычной Linux-сборки (build/).
# Использование: ./make-deb.sh [BUILD_DIR] — результат: qtiv_<версия>-1_amd64.deb
#
# Зависимости рантайма указаны с альтернативами: имена Qt-библиотек
# различаются между Debian 12 (libqt6core6) и Ubuntu 24.04+ (…t64).
# Пакету нужен Qt 6.8+; на более старых системах соберите из исходников.

set -euo pipefail
cd "$(dirname "$0")"

BUILD=${1:-../build}
VERSION=$(sed -n 's/^project(qtiv VERSION \([0-9.]*\).*/\1/p' ../CMakeLists.txt)
[ -n "$VERSION" ] || { echo "не удалось прочитать версию из CMakeLists.txt"; exit 1; }

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

DESTDIR="$STAGE" cmake --install "$BUILD" --prefix /usr
install -Dm644 ../LICENSE "$STAGE/usr/share/doc/qtiv/copyright"
install -Dm644 ../README.md "$STAGE/usr/share/doc/qtiv/README.md"

mkdir -p "$STAGE/DEBIAN"
cat > "$STAGE/DEBIAN/control" <<EOF
Package: qtiv
Version: ${VERSION}-1
Section: graphics
Priority: optional
Architecture: amd64
Depends: libc6, libqt6core6t64 | libqt6core6, libqt6gui6t64 | libqt6gui6, libqt6widgets6t64 | libqt6widgets6, libqt6svg6 | libqt6svg6t64
Recommends: qt6-imageformats
Maintainer: QTIV project
Description: Qt Image Viewer - view, compare and convert images
 Viewer with side-by-side comparison of up to 8 photos (synchronized zoom),
 format conversion and the open .qtivp album format. Ships the qtivp
 command-line tool for packing/unpacking albums and the qtivh help viewer.
 Requires Qt 6.8 or newer.
EOF

dpkg-deb --build --root-owner-group "$STAGE" "qtiv_${VERSION}-1_amd64.deb"
echo "готово: qtiv_${VERSION}-1_amd64.deb"

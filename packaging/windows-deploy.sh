#!/bin/bash
# Сборка портабельного каталога QTIV для Windows.
# Использование: windows-deploy.sh BUILD_DIR QT_PREFIX OUT_DIR
#   BUILD_DIR — кросс-сборка (build-win), QT_PREFIX — распакованный Qt для
#   Windows (~/qt-win). Результат — каталог, готовый к запуску и к NSIS.

set -euo pipefail
cd "$(dirname "$0")"

BUILD=${1:?"укажите каталог сборки, напр. ../build-win"}
QT=${2:?"укажите префикс Qt для Windows, напр. ~/qt-win"}
OUT=${3:-qtiv-win}

rm -rf "$OUT"
mkdir -p "$OUT"/{platforms,styles,imageformats,iconengines}

# Бинарники программы
for exe in qtiv qtivp qtivh; do
    cp "$BUILD/$exe.exe" "$OUT/"
done

# Qt и рантайм libc++ (llvm-mingw). opengl32sw/D3Dcompiler — софтверный
# OpenGL и компилятор шейдеров ANGLE-стека, на случай проблем с драйвером.
for dll in Qt6Core Qt6Gui Qt6Widgets Qt6Svg; do
    cp "$QT/bin/$dll.dll" "$OUT/"
done
for dll in opengl32sw.dll d3dcompiler_47.dll libc++.dll libunwind.dll; do
    cp "$QT/$dll" "$OUT/"
done

# Плагины: платформа, стиль, форматы изображений, SVG-иконки
cp "$QT/plugins/platforms/qwindows.dll" "$OUT/platforms/"
cp "$QT/plugins/styles/"*.dll "$OUT/styles/"
cp "$QT/plugins/imageformats/"*.dll "$OUT/imageformats/"
cp "$QT/plugins/iconengines/"*.dll "$OUT/iconengines/"

# Сопроводительные файлы
cp qtiv.ico "$OUT/qtiv.ico"
cp ../LICENSE "$OUT/LICENSE.txt"
cp ../README.md "$OUT/README.md"

echo "готово: $OUT"
du -sh "$OUT"
find "$OUT" -type f | wc -l

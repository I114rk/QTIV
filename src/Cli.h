#pragma once

#include <QString>
#include <QStringList>

// Консольные режимы (работают без GUI/платформы):
namespace Cli {

// --info FILE: информация об изображении или альбоме в JSON.
int info(const QString& path);

// --pack OUT.qtivp FILES...: упаковка изображений и альбомов в .qtivp.
int pack(const QString& outPath, const QStringList& inputs);

// -c, --cat FILES...: показать изображения в терминале
// (kitty/sixel графика либо символьный арт). renderMode: "", "ascii",
// "half", "sixel", "kitty".
int cat(const QStringList& paths, const QString& renderMode);

} // namespace Cli

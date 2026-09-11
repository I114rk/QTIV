#pragma once

#include <QImage>
#include <QString>

// Вывод изображений прямо в терминал (stdout). Режимы:
//   Kitty  — протокол kitty graphics (kitty, WezTerm, Ghostty, Konsole…)
//   Sixel  — DEC Sixel (foot, xterm, mintty…)
//   Half   — полублоки ▀ с truecolor/256-цветным фоном
//   Ascii  — ASCII-арт по яркости
namespace TermRender {

enum class Mode { Auto, Ascii, Half, Sixel, Kitty };

struct Caps {
    bool isTty = false;
    int cols = 80;
    int rows = 24;
    int pxWidth = 0;      // 0 = терминал не сообщил размер в пикселях
    int pxHeight = 0;
    bool kittyGraphics = false;
    bool sixel = false;
    bool truecolor = false;
    bool color256 = false;
};

// Определяет возможности терминала (env + активные запросы; только если
// stdout — tty). Безопасно вызывать и в пайпе — тогда isTty=false.
Caps detectCaps();

// Пишет изображение в stdout. Mode::Auto выбирает лучший режим по caps.
// Возвращает false, если вывести не удалось.
bool render(const QImage& image, Mode mode, const Caps& caps);

} // namespace TermRender

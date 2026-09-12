#include "TerminalRender.h"

#include <QBuffer>
#include <QHash>
#include <QImageWriter>
#include <QPainter>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <io.h>
#  include <unistd.h>
#else
#  include <poll.h>
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <unistd.h>
#endif

namespace {

void writeOut(const QByteArray& data)
{
    if (!data.isEmpty())
        std::fwrite(data.constData(), 1, size_t(data.size()), stdout);
    std::fflush(stdout);
}

QImage flatten(const QImage& image)
{
    if (image.hasAlphaChannel()) {
        QImage base(image.size(), QImage::Format_ARGB32);
        base.fill(Qt::black);
        QPainter p(&base);
        p.drawImage(0, 0, image);
        return base;
    }
    return image.convertToFormat(QImage::Format_ARGB32);
}

// --- Активный запрос к терминалу -----------------------------------------
// Пишем escape-последовательность в stdout и читаем ответ из stdin
// в raw-режиме с таймаутом. Не-терминал или нет ответа → пусто.

QByteArray queryTerminal(const QByteArray& query, int timeoutMs)
{
#if defined(_WIN32)
    // На Windows активные запросы к терминалу не выполняются: возможности
    // определяются по переменным окружения (WT_SESSION, TERM и т.п.).
    Q_UNUSED(query);
    Q_UNUSED(timeoutMs);
    return {};
#else
    if (isatty(STDIN_FILENO) != 1 || isatty(STDOUT_FILENO) != 1)
        return {};

    termios orig;
    if (tcgetattr(STDIN_FILENO, &orig) != 0)
        return {};

    termios raw = orig;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
        return {};

    tcflush(STDIN_FILENO, TCIFLUSH);
    if (::write(STDOUT_FILENO, query.constData(), size_t(query.size())) < 0) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        return {};
    }

    QByteArray response;
    pollfd pfd{STDIN_FILENO, POLLIN, 0};
    int elapsed = 0;
    while (elapsed < timeoutMs) {
        const int r = poll(&pfd, 1, 15);
        elapsed += 15;
        if (r > 0) {
            char buf[1024];
            const ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
            if (n > 0) {
                response.append(buf, int(n));
                // терминатор ответа (ST для APC, 'c' для DA1)
                if (response.contains('\x1b') && response.contains('c'))
                    break;
            }
        } else if (!response.isEmpty()) {
            break; // ответ получен и новых данных нет — хватит
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    return response;
#endif // !_WIN32
}

// --- Kitty graphics --------------------------------------------------------

bool renderKitty(const QImage& image, const TermRender::Caps& caps)
{
    const double cellW = caps.pxWidth > 0 ? double(caps.pxWidth) / caps.cols : 9.0;
    const double cellH = caps.pxHeight > 0 ? double(caps.pxHeight) / caps.rows : 18.0;
    const int maxW = std::max(8, caps.pxWidth > 0 ? caps.pxWidth : int(caps.cols * cellW));
    const int maxH = std::max(8, int((caps.rows - 2) * cellH));

    const QImage scaled = flatten(image).scaled(maxW, maxH, Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation);
    if (scaled.isNull() || scaled.width() < 1 || scaled.height() < 1)
        return false;

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, QByteArrayLiteral("png"));
    if (!writer.write(scaled))
        return false;

    const int cellsW = std::max(1, int(std::lround(scaled.width() / cellW)));
    const int cellsH = std::max(1, int(std::lround(scaled.height() / cellH)));

    const QByteArray b64 = png.toBase64();
    const int chunkSize = 4000;
    int offset = 0;
    bool first = true;
    do {
        const QByteArray part = b64.mid(offset, chunkSize);
        offset += part.size();
        const bool more = offset < b64.size();

        QByteArray params;
        if (first) {
            params = QStringLiteral("f=100,q=2,s=%1,v=%2,c=%3,r=%4,m=%5")
                         .arg(scaled.width())
                         .arg(scaled.height())
                         .arg(cellsW)
                         .arg(cellsH)
                         .arg(more ? 1 : 0)
                         .toLatin1();
            first = false;
        } else {
            params = QByteArrayLiteral("m=") + (more ? "1" : "0");
        }
        writeOut(QByteArrayLiteral("\x1b_G") + params + QByteArrayLiteral(";")
                 + part + QByteArrayLiteral("\x1b\\"));
    } while (offset < b64.size());

    QByteArray tail;
    for (int i = 0; i < cellsH; ++i)
        tail += '\n';
    writeOut(tail);
    return true;
}

// --- Sixel ------------------------------------------------------------------

QVector<QRgb> medianCutPalette(const QImage& image, int maxColors)
{
    QList<QList<QRgb>> boxes;
    {
        QList<QRgb> pixels;
        const qint64 total = qint64(image.width()) * image.height();
        const int step = int(std::max<qreal>(1.0, std::sqrt(qreal(total) / 20000.0)));
        for (int y = 0; y < image.height(); y += step)
            for (int x = 0; x < image.width(); x += step)
                pixels.append(image.pixel(x, y));
        if (pixels.isEmpty())
            return { qRgb(0, 0, 0) };
        boxes.append(pixels);
    }

    while (boxes.size() < maxColors) {
        int bestBox = -1, bestChannel = -1, bestRange = 0;
        for (int b = 0; b < boxes.size(); ++b) {
            const QList<QRgb>& px = boxes[b];
            if (px.size() < 2)
                continue;
            int mn[3] = { 255, 255, 255 }, mx[3] = { 0, 0, 0 };
            for (QRgb c : px) {
                mn[0] = std::min(mn[0], qRed(c));
                mx[0] = std::max(mx[0], qRed(c));
                mn[1] = std::min(mn[1], qGreen(c));
                mx[1] = std::max(mx[1], qGreen(c));
                mn[2] = std::min(mn[2], qBlue(c));
                mx[2] = std::max(mx[2], qBlue(c));
            }
            for (int ch = 0; ch < 3; ++ch) {
                const int range = mx[ch] - mn[ch];
                if (range > bestRange) {
                    bestRange = range;
                    bestBox = b;
                    bestChannel = ch;
                }
            }
        }
        if (bestBox < 0 || bestRange == 0)
            break;

        QList<QRgb>& px = boxes[bestBox];
        std::sort(px.begin(), px.end(), [bestChannel](QRgb a, QRgb b) {
            const int ca = bestChannel == 0 ? qRed(a) : (bestChannel == 1 ? qGreen(a) : qBlue(a));
            const int cb = bestChannel == 0 ? qRed(b) : (bestChannel == 1 ? qGreen(b) : qBlue(b));
            return ca < cb;
        });
        const int mid = px.size() / 2;
        QList<QRgb> tail = px.mid(mid);
        px.erase(px.begin() + mid, px.end());
        if (tail.isEmpty() || px.isEmpty())
            break;
        boxes.append(tail);
    }

    QVector<QRgb> palette;
    palette.reserve(boxes.size());
    for (const QList<QRgb>& px : boxes) {
        if (px.isEmpty())
            continue;
        qint64 r = 0, g = 0, b = 0;
        for (QRgb c : px) {
            r += qRed(c);
            g += qGreen(c);
            b += qBlue(c);
        }
        palette.append(qRgb(int(r / px.size()), int(g / px.size()), int(b / px.size())));
    }
    return palette;
}

void appendSixelBand(QByteArray& out, const QByteArray& band)
{
    // RLE-сжатие повторов: '!' количество символ
    int i = 0;
    while (i < band.size()) {
        const char c = band[i];
        int run = 1;
        while (i + run < band.size() && band[i + run] == c)
            ++run;
        if (run >= 4) {
            out += '!';
            out += QByteArray::number(run);
            out += c;
        } else {
            out += QByteArray(run, c);
        }
        i += run;
    }
}

bool renderSixel(const QImage& image, const TermRender::Caps& caps)
{
    const double cellW = caps.pxWidth > 0 ? double(caps.pxWidth) / caps.cols : 9.0;
    const double cellH = caps.pxHeight > 0 ? double(caps.pxHeight) / caps.rows : 18.0;
    const int maxW = std::max(8, caps.pxWidth > 0 ? caps.pxWidth : int(caps.cols * cellW));
    const int maxH = std::max(8, int((caps.rows - 2) * cellH));

    const QImage scaled = flatten(image).scaled(maxW, maxH, Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation);
    if (scaled.isNull() || scaled.width() < 1 || scaled.height() < 1)
        return false;
    const int w = scaled.width();
    const int h = scaled.height();

    const QVector<QRgb> palette = medianCutPalette(scaled, 128);
    if (palette.isEmpty())
        return false;

    // Индекс ближайшего цвета палитры для каждого пикселя.
    QVector<quint8> indices(w * h);
    {
        QHash<QRgb, int> nearest;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const QRgb c = scaled.pixel(x, y);
                const auto it = nearest.constFind(c);
                if (it != nearest.constEnd()) {
                    indices[y * w + x] = quint8(it.value());
                    continue;
                }
                int best = 0;
                qint64 bestDist = INT64_MAX;
                for (int i = 0; i < palette.size(); ++i) {
                    const qint64 dr = qint64(qRed(c)) - qRed(palette[i]);
                    const qint64 dg = qint64(qGreen(c)) - qGreen(palette[i]);
                    const qint64 db = qint64(qBlue(c)) - qBlue(palette[i]);
                    const qint64 dist = dr * dr + dg * dg + db * db;
                    if (dist < bestDist) {
                        bestDist = dist;
                        best = i;
                    }
                }
                nearest.insert(c, best);
                indices[y * w + x] = quint8(best);
            }
        }
    }

    QByteArray out;
    out.reserve(w * h / 2 + 4096);
    out += QByteArrayLiteral("\x1bPq");
    out += QByteArrayLiteral("\"1;1;") + QByteArray::number(w) + ';'
        + QByteArray::number(h);
    for (int i = 0; i < palette.size(); ++i) {
        const QRgb c = palette[i];
        out += '#' + QByteArray::number(i) + QByteArrayLiteral(";2;")
             + QByteArray::number(qRound(qRed(c) * 100.0 / 255.0)) + ';'
             + QByteArray::number(qRound(qGreen(c) * 100.0 / 255.0)) + ';'
             + QByteArray::number(qRound(qBlue(c) * 100.0 / 255.0));
    }

    for (int y0 = 0; y0 < h; y0 += 6) {
        const int bandH = std::min(6, h - y0);
        QList<int> usedColors;
        {
            QSet<int> used;
            for (int dy = 0; dy < bandH; ++dy)
                for (int x = 0; x < w; ++x)
                    used.insert(int(indices[(y0 + dy) * w + x]));
            usedColors = used.values();
            std::sort(usedColors.begin(), usedColors.end());
        }
        bool firstInBand = true;
        for (const int color : usedColors) {
            QByteArray band;
            band.reserve(w);
            for (int x = 0; x < w; ++x) {
                int bits = 0;
                for (int dy = 0; dy < bandH; ++dy) {
                    if (int(indices[(y0 + dy) * w + x]) == color)
                        bits |= 1 << dy;
                }
                band += bits == 0 ? '?' : char(0x3F + bits);
            }
            if (!firstInBand)
                out += '$';
            firstInBand = false;
            out += '#';
            out += QByteArray::number(color);
            appendSixelBand(out, band);
        }
        out += '-';
    }
    out += QByteArrayLiteral("\x1b\\");
    writeOut(out);
    writeOut(QByteArrayLiteral("\n"));
    return true;
}

// --- Полублоки ----------------------------------------------------------------

int nearestColor256(int r, int g, int b)
{
    // Приближение к палитре xterm-256: куб 6×6×6 (16..231) и серая шкала (232..255).
    const int gray = (r + g + b) / 3;
    const int grayIdx = qBound(0, (gray - 3) / 10, 23);
    const int grayVal = 8 + 10 * grayIdx;
    const qint64 grayDist = qint64(r - grayVal) * (r - grayVal)
        + qint64(g - grayVal) * (g - grayVal)
        + qint64(b - grayVal) * (b - grayVal);

    const int nr = qBound(0, (r * 5 + 127) / 255, 5);
    const int ng = qBound(0, (g * 5 + 127) / 255, 5);
    const int nb = qBound(0, (b * 5 + 127) / 255, 5);
    const int vr = nr == 0 ? 0 : nr * 40 + 55;
    const int vg = ng == 0 ? 0 : ng * 40 + 55;
    const int vb = nb == 0 ? 0 : nb * 40 + 55;
    const qint64 cubeDist = qint64(r - vr) * (r - vr)
        + qint64(g - vg) * (g - vg)
        + qint64(b - vb) * (b - vb);

    if (grayDist <= cubeDist)
        return 232 + grayIdx;
    return 16 + 36 * nr + 6 * ng + nb;
}

void appendFgColor(QByteArray& out, QRgb c, bool truecolor)
{
    if (truecolor) {
        out += QStringLiteral("\x1b[38;2;%1;%2;%3m")
                   .arg(qRed(c))
                   .arg(qGreen(c))
                   .arg(qBlue(c))
                   .toLatin1();
    } else {
        out += QByteArrayLiteral("\x1b[38;5;")
             + QByteArray::number(nearestColor256(qRed(c), qGreen(c), qBlue(c))) + 'm';
    }
}

void appendBgColor(QByteArray& out, QRgb c, bool truecolor)
{
    if (truecolor) {
        out += QStringLiteral("\x1b[48;2;%1;%2;%3m")
                   .arg(qRed(c))
                   .arg(qGreen(c))
                   .arg(qBlue(c))
                   .toLatin1();
    } else {
        out += QByteArrayLiteral("\x1b[48;5;")
             + QByteArray::number(nearestColor256(qRed(c), qGreen(c), qBlue(c))) + 'm';
    }
}

bool renderHalf(const QImage& image, const TermRender::Caps& caps)
{
    if (caps.cols < 8 || caps.rows < 3)
        return false;
    const int charCols = caps.cols - 1;
    const int charRows = caps.rows - 1;

    const QImage scaled = flatten(image).scaled(charCols, charRows * 2, Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation);
    if (scaled.isNull() || scaled.width() < 1 || scaled.height() < 1)
        return false;
    const int w = scaled.width();
    const int h = scaled.height();
    const bool truecolor = caps.truecolor;

    QByteArray out;
    out.reserve(w * h * 6);
    for (int y = 0; y < h; y += 2) {
        QRgb fg = 0, bg = 0;
        bool haveColors = false;
        for (int x = 0; x < w; ++x) {
            const QRgb top = scaled.pixel(x, y);
            const QRgb bottom = (y + 1 < h) ? scaled.pixel(x, y + 1) : top;
            if (!haveColors || top != fg || bottom != bg) {
                appendFgColor(out, top, truecolor);
                appendBgColor(out, bottom, truecolor);
                fg = top;
                bg = bottom;
                haveColors = true;
            }
            out += QByteArrayLiteral("\xE2\x96\x80"); // ▀
        }
        out += QByteArrayLiteral("\x1b[0m\n");
    }
    writeOut(out);
    return true;
}

// --- ASCII --------------------------------------------------------------------

bool renderAscii(const QImage& image, const TermRender::Caps& caps, bool colorize)
{
    static const char ramp[] = " .:-=+*#%@";
    const int rampLen = int(std::strlen(ramp));

    const int charCols = caps.isTty ? std::max(8, caps.cols - 1) : 80;
    const int targetH = std::max(1, qRound(double(charCols) * image.height() / image.width() / 2.0));
    QImage scaled = flatten(image).scaled(charCols, targetH, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
    if (scaled.isNull())
        return false;
    if (caps.isTty && scaled.height() > caps.rows - 1)
        scaled = scaled.scaledToHeight(std::max(1, caps.rows - 1), Qt::SmoothTransformation);

    const int w = scaled.width();
    const int h = scaled.height();
    QByteArray out;
    out.reserve(w * h * 4);
    QRgb lastColor = 0;
    bool have = false;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const QRgb c = scaled.pixel(x, y);
            const char ch = ramp[qBound(0, qGray(c) * rampLen / 256, rampLen - 1)];
            if (colorize) {
                if (!have || c != lastColor) {
                    appendFgColor(out, c, caps.truecolor);
                    lastColor = c;
                    have = true;
                }
            }
            out += ch;
        }
        if (colorize)
            out += QByteArrayLiteral("\x1b[0m");
        out += '\n';
    }
    writeOut(out);
    return true;
}

} // namespace

namespace TermRender {

Caps detectCaps()
{
    Caps caps;
    caps.isTty = isatty(STDOUT_FILENO) == 1;

#if defined(_WIN32)
    if (CONSOLE_SCREEN_BUFFER_INFO info{};
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        caps.cols = qMax(1, int(info.dwSize.X));
        caps.rows = qMax(1, int(info.srWindow.Bottom - info.srWindow.Top) + 1);
    }
#else
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        caps.cols = int(ws.ws_col);
        caps.rows = int(ws.ws_row);
        if (ws.ws_xpixel > 0 && ws.ws_ypixel > 0) {
            caps.pxWidth = int(ws.ws_xpixel);
            caps.pxHeight = int(ws.ws_ypixel);
        }
    }
#endif

    const QByteArray term = qgetenv("TERM");
    const QByteArray colorterm = qgetenv("COLORTERM");
    // Windows Terminal всегда поддерживает truecolor.
    const bool windowsTerminal = !qgetenv("WT_SESSION").isEmpty();
    caps.truecolor = windowsTerminal || colorterm.contains("truecolor")
        || colorterm.contains("24bit");
    caps.color256 = caps.truecolor || windowsTerminal || term.contains("256color")
        || term.contains("kitty") || term.contains("alacritty") || term.contains("ghostty")
        || term.contains("wezterm") || term.contains("foot");

    if (!caps.isTty)
        return caps;

    // tmux не пропускает APC/DCS-последовательности (kitty/sixel) наружу —
    // внутри мультиплексора честно работают только полублоки и ASCII.
    if (!qgetenv("TMUX").isEmpty())
        return caps;

    // Активный запрос поддержки kitty graphics (ответ приходит на stdin).
    const QByteArray kittyReply =
        queryTerminal(QByteArrayLiteral("\x1b_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\x1b\\"), 150);
    caps.kittyGraphics = kittyReply.contains("\x1b_Gi=31;OK");

    if (!caps.kittyGraphics) {
        // DA1: параметр 4 = поддержка sixel.
        const QByteArray da = queryTerminal(QByteArrayLiteral("\x1b[c"), 150);
        if (da.contains("\x1b[?")) {
            const int start = da.indexOf("\x1b[?") + 3; // длина "\x1b[?"
            const int end = da.indexOf('c', start);
            if (end > start) {
                const QList<QByteArray> params = da.mid(start, end - start).split(';');
                caps.sixel = params.contains(QByteArrayLiteral("4"));
            }
        }
    }

    // Fallback по переменным окружения, если терминал не отвечает на запросы.
    if (!caps.kittyGraphics) {
        const QByteArray termProgram = qgetenv("TERM_PROGRAM");
        if (term == "xterm-kitty" || termProgram == "WezTerm" || termProgram == "ghostty"
            || term.contains("ghostty") || !qgetenv("KONSOLE_VERSION").isEmpty())
            caps.kittyGraphics = true;
    }
    if (!caps.sixel && !caps.kittyGraphics) {
        const QByteArray termProgram = qgetenv("TERM_PROGRAM");
        if (term.contains("foot") || termProgram == "mintty" || term.contains("yaft"))
            caps.sixel = true;
    }
    return caps;
}

namespace {

bool renderOne(const QImage& image, Mode m, const Caps& caps)
{
    switch (m) {
    case Mode::Kitty:
        return renderKitty(image, caps);
    case Mode::Sixel:
        return renderSixel(image, caps);
    case Mode::Half:
        return renderHalf(image, caps);
    case Mode::Ascii:
        return renderAscii(image, caps, caps.isTty && (caps.truecolor || caps.color256));
    case Mode::Auto:
        break;
    }
    return false;
}

} // namespace

bool render(const QImage& image, Mode mode, const Caps& caps)
{
    if (image.isNull())
        return false;

    Mode primary;
    if (!caps.isTty) {
        // Пайп или файл: только чистый ASCII без escape-последовательностей.
        return renderOne(image, Mode::Ascii, caps);
    } else if (mode == Mode::Auto) {
        if (caps.kittyGraphics)
            primary = Mode::Kitty;
        else if (caps.sixel)
            primary = Mode::Sixel;
        else if (caps.truecolor || caps.color256)
            primary = Mode::Half;
        else
            primary = Mode::Ascii;
    } else {
        primary = mode;
    }

    // Основной режим; при неудаче — деградация до гарантированно работающего.
    const Mode chain[] = {primary, Mode::Sixel, Mode::Half, Mode::Ascii};
    for (const Mode m : chain) {
        if (renderOne(image, m, caps))
            return true;
    }
    return false;
}

} // namespace TermRender

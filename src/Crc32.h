#pragma once

// CRC-32/ISO-HDLC (zlib-compatible, polynomial 0xEDB88320, reflected).
// Used by the .qtivp container for per-blob integrity (see docs/QTIVP-SPEC.md).
#include <QByteArray>
#include <QtGlobal>

namespace Qtivp {

inline quint32 crc32(const char* data, qsizetype length)
{
    static quint32 table[256];
    static bool tableReady = false;
    if (!tableReady) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        tableReady = true;
    }
    quint32 c = 0xFFFFFFFFu;
    for (qsizetype i = 0; i < length; ++i)
        c = table[(c ^ quint8(data[i])) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

inline quint32 crc32(const QByteArray& data)
{
    return crc32(data.constData(), data.size());
}

} // namespace Qtivp

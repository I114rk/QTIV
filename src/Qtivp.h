#pragma once

// QTIVP (.qtivp) — открытый контейнерный формат для хранения нескольких
// фотографий в одном файле. Полная спецификация: docs/QTIVP-SPEC.md.
//
// Структура (все числа little-endian):
//   Заголовок:  magic "QTIVP1\0\0" (8 байт) + version u16 + flags u16 + count u32
//   Индекс:     count записей: name(utf-8) + mime(ascii) + width/height u32
//               + rawSize u32 + storedSize u32 + offset u64 + flags u32 + crc32 u32
//   Данные:     blob'ы по смещениям; blob может быть zlib-сжат (FlagZlib).

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>

namespace Qtivp {

inline QByteArray magic() { return QByteArray("QTIVP1\0\0", 8); }

constexpr quint16 Version = 1;
constexpr quint32 FlagZlib = 0x1;

struct Entry {
    // Метаданные (общие для чтения и записи).
    QString name;          // исходное имя файла, UTF-8
    QString mime;          // например "image/jpeg"
    quint32 width = 0;     // пиксели, информационно (0 = неизвестно)
    quint32 height = 0;

    // Заполнение при записи: несжатые исходные байты.
    QByteArray blob;

    // Заполняется при чтении индекса.
    quint32 rawSize = 0;       // размер несжатого blob'а
    quint32 storedSize = 0;    // размер как хранится (сжатый или нет)
    quint64 offset = 0;        // абсолютное смещение в файле
    quint32 flags = 0;         // FlagZlib, если сжат
    quint32 crc32 = 0;         // CRC-32 несжатого blob'а
};

using EntryList = QList<Entry>;

class Writer {
public:
    // Собирает альбом из записей (нужны name, mime, blob; width/height — опционально).
    // rawSize и crc32 вычисляются автоматически. Сжатие zlib применяется,
    // если оно действительно уменьшает blob.
    static bool write(const QString& path, const EntryList& entries, QString* error = nullptr);
};

class Reader {
public:
    // Читает только заголовок и индекс — быстро даже для больших альбомов.
    static bool openIndex(const QString& path, EntryList& entries, QString* error = nullptr);

    // Читает, распаковывает и проверяет CRC одного blob'а.
    static QByteArray readBlob(const QString& path, const Entry& entry, QString* error = nullptr);

    // Читает blob и декодирует в QImage (с учётом EXIF-ориентации).
    static QImage decode(const QString& path, const Entry& entry, QString* error = nullptr);
};

// Утилиты для сопоставления форматов и MIME-типов.
QString mimeForFormat(const QString& format);
QString formatForMime(const QString& mime);

} // namespace Qtivp

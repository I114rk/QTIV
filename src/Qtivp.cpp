#include "Qtivp.h"
#include "Crc32.h"

#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QHash>
#include <QImageReader>
#include <QSaveFile>

namespace Qtivp {
namespace {

constexpr qint64 kHeaderSize = 16; // magic(8) + version(2) + flags(2) + count(4)

} // namespace

QString mimeForFormat(const QString& format)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("png"),   QStringLiteral("image/png")},
        {QStringLiteral("jpg"),   QStringLiteral("image/jpeg")},
        {QStringLiteral("jpeg"),  QStringLiteral("image/jpeg")},
        {QStringLiteral("webp"),  QStringLiteral("image/webp")},
        {QStringLiteral("avif"),  QStringLiteral("image/avif")},
        {QStringLiteral("bmp"),   QStringLiteral("image/bmp")},
        {QStringLiteral("gif"),   QStringLiteral("image/gif")},
        {QStringLiteral("tiff"),  QStringLiteral("image/tiff")},
        {QStringLiteral("tif"),   QStringLiteral("image/tiff")},
        {QStringLiteral("heic"),  QStringLiteral("image/heic")},
        {QStringLiteral("qtivp"), QStringLiteral("application/x-qtivp")},
    };
    return map.value(format.toLower(), QStringLiteral("application/octet-stream"));
}

QString formatForMime(const QString& mime)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("image/png"),   QStringLiteral("png")},
        {QStringLiteral("image/jpeg"),  QStringLiteral("jpeg")},
        {QStringLiteral("image/webp"),  QStringLiteral("webp")},
        {QStringLiteral("image/avif"),  QStringLiteral("avif")},
        {QStringLiteral("image/bmp"),   QStringLiteral("bmp")},
        {QStringLiteral("image/gif"),   QStringLiteral("gif")},
        {QStringLiteral("image/tiff"),  QStringLiteral("tiff")},
        {QStringLiteral("image/heic"),  QStringLiteral("heic")},
    };
    return map.value(mime.toLower());
}

bool Writer::write(const QString& path, const EntryList& entries, QString* error)
{
    auto fail = [error](const QString& msg) {
        if (error)
            *error = msg;
        return false;
    };

    struct Prepared {
        QByteArray nameUtf8;
        QByteArray mimeAscii;
        QByteArray storedBytes;
        quint32 rawSize = 0;
        quint32 storedSize = 0;
        quint64 offset = 0;
        quint32 flags = 0;
        quint32 crc = 0;
        quint32 width = 0;
        quint32 height = 0;
    };

    QList<Prepared> prepared;
    prepared.reserve(entries.size());

    quint64 indexSize = 0;
    for (const Entry& e : entries) {
        Prepared p;
        p.nameUtf8 = e.name.toUtf8();
        p.mimeAscii = e.mime.toLatin1();
        if (p.nameUtf8.size() > 0xFFFF)
            return fail(QStringLiteral("entry name too long: %1").arg(e.name));
        if (p.mimeAscii.size() > 0xFFFF)
            return fail(QStringLiteral("entry mime too long: %1").arg(e.mime));

        p.rawSize = quint32(e.blob.size());
        p.crc = crc32(e.blob);
        p.width = e.width;
        p.height = e.height;

        // zlib имеет смысл только когда реально уменьшает данные.
        if (e.blob.size() > 512) {
            const QByteArray compressed = qCompress(e.blob);
            if (!compressed.isEmpty()
                && quint64(compressed.size()) + 256 < quint64(e.blob.size())) {
                p.storedBytes = compressed;
                p.flags = FlagZlib;
            }
        }
        if (p.storedBytes.isNull())
            p.storedBytes = e.blob;
        p.storedSize = quint32(p.storedBytes.size());

        indexSize += quint64(2 + p.nameUtf8.size() + 2 + p.mimeAscii.size() + 32);
        prepared.append(p);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("could not open %1 for writing: %2")
                        .arg(path, file.errorString()));

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds.writeRawData(magic().constData(), 8);
    ds << Version << quint16(0) << quint32(entries.size());

    quint64 offset = quint64(kHeaderSize) + indexSize;
    for (Prepared& p : prepared) {
        p.offset = offset;
        offset += p.storedSize;

        ds << quint16(p.nameUtf8.size());
        ds.writeRawData(p.nameUtf8.constData(), p.nameUtf8.size());
        ds << quint16(p.mimeAscii.size());
        ds.writeRawData(p.mimeAscii.constData(), p.mimeAscii.size());
        ds << p.width << p.height << p.rawSize << p.storedSize << p.offset << p.flags << p.crc;
    }

    for (const Prepared& p : prepared)
        ds.writeRawData(p.storedBytes.constData(), p.storedBytes.size());

    if (ds.status() != QDataStream::Ok) {
        file.cancelWriting();
        return fail(QStringLiteral("I/O error while writing %1").arg(path));
    }
    if (!file.commit())
        return fail(QStringLiteral("could not write %1: %2").arg(path, file.errorString()));
    return true;
}

bool Reader::openIndex(const QString& path, EntryList& entries, QString* error)
{
    auto fail = [error](const QString& msg) {
        if (error)
            *error = msg;
        return false;
    };
    entries.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("could not open %1: %2").arg(path, file.errorString()));
    const qint64 fileSize = file.size();

    if (fileSize < kHeaderSize || file.read(8) != magic())
        return fail(QStringLiteral("%1 is not a QTIVP album").arg(path));

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint16 version = 0, flags = 0;
    quint32 count = 0;
    ds >> version >> flags >> count;
    if (ds.status() != QDataStream::Ok)
        return fail(QStringLiteral("%1: unexpected end of file").arg(path));
    if (version > Version)
        return fail(QStringLiteral("%1: album version %2 is not supported (this build supports %3)")
                        .arg(path).arg(version).arg(Version));

    entries.reserve(int(count));
    for (quint32 i = 0; i < count; ++i) {
        quint16 nameLen = 0, mimeLen = 0;
        ds >> nameLen;
        QByteArray name(nameLen, '\0');
        if (nameLen && ds.readRawData(name.data(), nameLen) != nameLen)
            return fail(QStringLiteral("%1: unexpected end of file").arg(path));
        ds >> mimeLen;
        QByteArray mime(mimeLen, '\0');
        if (mimeLen && ds.readRawData(mime.data(), mimeLen) != mimeLen)
            return fail(QStringLiteral("%1: unexpected end of file").arg(path));

        Entry e;
        e.name = QString::fromUtf8(name);
        e.mime = QString::fromLatin1(mime);
        ds >> e.width >> e.height >> e.rawSize >> e.storedSize >> e.offset >> e.flags >> e.crc32;
        if (ds.status() != QDataStream::Ok)
            return fail(QStringLiteral("%1: unexpected end of file").arg(path));
        if (e.offset > quint64(fileSize) || e.storedSize > quint64(fileSize) - e.offset)
            return fail(QStringLiteral("%1: album index is corrupt").arg(path));
        entries.append(e);
    }
    return true;
}

QByteArray Reader::readBlob(const QString& path, const Entry& entry, QString* error)
{
    auto fail = [error](const QString& msg) {
        if (error)
            *error = msg;
        return QByteArray();
    };

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("could not open %1: %2").arg(path, file.errorString()));
    if (!file.seek(qint64(entry.offset)))
        return fail(QStringLiteral("%1: seek failed").arg(path));

    QByteArray stored = file.read(entry.storedSize);
    if (stored.size() != int(entry.storedSize))
        return fail(QStringLiteral("%1: blob data truncated").arg(path));

    QByteArray raw = stored;
    if (entry.flags & FlagZlib) {
        raw = qUncompress(stored);
        if (raw.isEmpty() && entry.rawSize != 0)
            return fail(QStringLiteral("%1: blob decompression failed").arg(path));
    }
    if (entry.rawSize != 0 && quint32(raw.size()) != entry.rawSize)
        return fail(QStringLiteral("%1: blob size mismatch").arg(path));
    if (entry.crc32 != 0 && crc32(raw) != entry.crc32)
        return fail(QStringLiteral("%1: blob checksum mismatch").arg(path));
    return raw;
}

QImage Reader::decode(const QString& path, const Entry& entry, QString* error)
{
    QByteArray raw = readBlob(path, entry, error);
    if (raw.isEmpty() && entry.rawSize != 0)
        return {};

    QBuffer buffer(&raw);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);
    QImage image = reader.read();
    if (image.isNull() && error)
        *error = QStringLiteral("%1: %2").arg(entry.name, reader.errorString());
    return image;
}

} // namespace Qtivp

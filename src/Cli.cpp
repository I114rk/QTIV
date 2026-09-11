#include "Cli.h"
#include "ImageStore.h"
#include "Qtivp.h"
#include "TerminalRender.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cstdio>

namespace {
QTextStream& out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream& err()
{
    static QTextStream stream(stderr);
    return stream;
}
} // namespace

int Cli::info(const QString& path)
{
    const QFileInfo fi(path);
    if (!fi.exists()) {
        err() << "error: no such file: " << path << Qt::endl;
        return 2;
    }

    QJsonObject root;
    root[QStringLiteral("path")] = fi.absoluteFilePath();

    if (ImageStore::isAlbumPath(path)) {
        Qtivp::EntryList entries;
        QString error;
        if (!Qtivp::Reader::openIndex(path, entries, &error)) {
            err() << "error: " << error << Qt::endl;
            return 3;
        }
        root[QStringLiteral("type")] = QStringLiteral("album");
        root[QStringLiteral("version")] = int(Qtivp::Version);
        root[QStringLiteral("count")] = entries.size();
        QJsonArray arr;
        for (const Qtivp::Entry& e : entries) {
            QJsonObject o;
            o[QStringLiteral("name")] = e.name;
            o[QStringLiteral("mime")] = e.mime;
            o[QStringLiteral("width")] = int(e.width);
            o[QStringLiteral("height")] = int(e.height);
            o[QStringLiteral("rawSize")] = qint64(e.rawSize);
            o[QStringLiteral("storedSize")] = qint64(e.storedSize);
            o[QStringLiteral("compressed")] = bool(e.flags & Qtivp::FlagZlib);
            o[QStringLiteral("crc32")] = QStringLiteral("0x%1").arg(e.crc32, 8, 16, QChar('0'));
            arr.append(o);
        }
        root[QStringLiteral("entries")] = arr;
    } else {
        QImageReader reader(path);
        const QSize sz = reader.size();
        if (!sz.isValid()) {
            err() << "error: cannot read image: " << path << Qt::endl;
            return 3;
        }
        root[QStringLiteral("type")] = QStringLiteral("image");
        root[QStringLiteral("format")] = QString::fromLatin1(reader.format());
        root[QStringLiteral("width")] = sz.width();
        root[QStringLiteral("height")] = sz.height();
        root[QStringLiteral("bytes")] = qint64(fi.size());
    }

    out() << QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)) << Qt::endl;
    return 0;
}

int Cli::pack(const QString& outPath, const QStringList& inputs)
{
    if (inputs.isEmpty()) {
        err() << "error: --pack requires input files" << Qt::endl;
        return 2;
    }

    Qtivp::EntryList entries;
    QStringList errors;

    for (const QString& input : inputs) {
        const QFileInfo fi(input);
        if (!fi.exists()) {
            errors << input + QStringLiteral(": no such file");
            continue;
        }

        if (ImageStore::isAlbumPath(input)) {
            // Альбом разворачивается в записи (без перекодирования).
            Qtivp::EntryList sub;
            QString error;
            if (!Qtivp::Reader::openIndex(input, sub, &error)) {
                errors << error;
                continue;
            }
            for (Qtivp::Entry& e : sub) {
                QString blobError;
                e.blob = Qtivp::Reader::readBlob(input, e, &blobError);
                if (e.blob.isEmpty()) {
                    errors << input + QLatin1Char('/') + e.name + QStringLiteral(": ")
                        + blobError;
                    continue;
                }
                if (e.name.isEmpty())
                    e.name = QStringLiteral("image");
                entries.append(e);
            }
            continue;
        }

        if (!ImageStore::isSupportedPath(input)) {
            errors << input + QStringLiteral(": unsupported file type");
            continue;
        }

        Qtivp::Entry e;
        e.name = fi.fileName();
        e.mime = Qtivp::mimeForFormat(fi.suffix());
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) {
            errors << input + QStringLiteral(": ") + f.errorString();
            continue;
        }
        e.blob = f.readAll();
        if (e.blob.isEmpty()) {
            errors << input + QStringLiteral(": empty file");
            continue;
        }

        QImageReader reader(fi.absoluteFilePath());
        QSize sz = reader.size();
        if (sz.isValid()) {
            if (reader.transformation() & QImageIOHandler::TransformationRotate90)
                sz.transpose();
            e.width = quint32(sz.width());
            e.height = quint32(sz.height());
        }
        entries.append(e);
    }

    if (entries.isEmpty()) {
        for (const QString& e : errors)
            err() << "error: " << e << Qt::endl;
        return 3;
    }

    QString error;
    if (!Qtivp::Writer::write(outPath, entries, &error)) {
        err() << "error: " << error << Qt::endl;
        return 3;
    }

    out() << "OK: packed " << entries.size() << " photo(s) into " << outPath << " ("
          << QFileInfo(outPath).size() / 1024 << " KiB)" << Qt::endl;
    for (const QString& e : errors)
        err() << "warning: " << e << Qt::endl;
    return 0;
}

int Cli::cat(const QStringList& paths, const QString& renderMode)
{
    TermRender::Mode mode = TermRender::Mode::Auto;
    if (renderMode == QLatin1String("ascii"))
        mode = TermRender::Mode::Ascii;
    else if (renderMode == QLatin1String("half"))
        mode = TermRender::Mode::Half;
    else if (renderMode == QLatin1String("sixel"))
        mode = TermRender::Mode::Sixel;
    else if (renderMode == QLatin1String("kitty"))
        mode = TermRender::Mode::Kitty;
    else if (!renderMode.isEmpty()) {
        err() << "error: unknown render mode: " << renderMode << Qt::endl;
        return 2;
    }

    if (paths.isEmpty()) {
        err() << "error: no files given" << Qt::endl;
        return 2;
    }

    const TermRender::Caps caps = TermRender::detectCaps();

    // Один обычный файл — показываем только его (без плейлиста папки).
    if (paths.size() == 1 && QFileInfo(paths.first()).isFile()
        && !ImageStore::isAlbumPath(paths.first())) {
        QImageReader reader(paths.first());
        reader.setAutoTransform(true);
        const QImage img = reader.read();
        if (img.isNull()) {
            err() << "error: " << reader.errorString() << Qt::endl;
            return 3;
        }
        TermRender::render(img, mode, caps);
        return 0;
    }

    // Несколько файлов, папки и альбомы — через плейлист.
    ImageStore store;
    const QString error = store.openPaths(paths);
    if (!error.isEmpty()) {
        err() << "error: " << error << Qt::endl;
        return 3;
    }

    bool any = false;
    for (int i = 0; i < store.count(); ++i) {
        const ImageStore::Item& it = store.item(i);
        const QImage img = store.image(i);
        if (img.isNull()) {
            err() << "warning: could not decode " << it.displayName << Qt::endl;
            continue;
        }
        if (store.count() > 1) {
            out() << QStringLiteral("%1 (%2x%3)").arg(it.displayName)
                         .arg(img.width())
                         .arg(img.height())
                  << Qt::endl;
        }
        TermRender::render(img, mode, caps);
        any = true;
    }
    return any ? 0 : 4;
}

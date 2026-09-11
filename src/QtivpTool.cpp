// qtivp — консольный инструмент для альбомов .qtivp:
//   qtivp -compress OUT.qtivp FILES...   упаковать фото (и альбомы, и папки) в альбом
//   qtivp -extract ALBUM [DIR]           распаковать альбом в папку (по умолчанию — рядом)
//   qtivp -list ALBUM                    показать содержимое альбома
//   qtivp -test ALBUM                    проверить структуру и CRC всех записей
// Переиспользует читатель/писатель формата из QTIV.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <cstdio>

#include "Cli.h"
#include "ImageStore.h"
#include "Qtivp.h"

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

void printUsage()
{
    out() << "qtivp " QTIV_VERSION " — QTIVP album tool\n"
        << "\n"
        << "Usage:\n"
        << "  qtivp -compress OUT.qtivp FILES...   Pack photos into an album\n"
        << "                                        (images, .qtivp albums and folders)\n"
        << "  qtivp -extract ALBUM.qtivp [DIR]     Extract all photos to DIR\n"
        << "  qtivp -list ALBUM.qtivp              List album contents\n"
        << "  qtivp -test ALBUM.qtivp              Verify structure and CRC32 of every entry\n"
        << "  qtivp -h | -v                        Help / version\n"
        << "\n"
        << "Examples:\n"
        << "  qtivp -compress photos.qtivp photo1.png photo2.png photo100.png\n"
        << "  qtivp -compress photos.qtivp photo*.*\n"
        << "  qtivp -compress vacation.qtivp ~/Pictures/sea/\n"
        << "  qtivp -extract photos.qtivp ~/out\n"
        << "  qtivp -test photos.qtivp\n";
    out().flush();
}

QString humanSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KiB").arg(bytes / 1024);
    return QStringLiteral("%1 MiB").arg(bytes / 1024 / 1024);
}

// Разворачивает папки в список изображений (папка — не рекурсивно).
QStringList expandInputs(const QStringList& inputs)
{
    QStringList files;
    for (const QString& input : inputs) {
        const QFileInfo fi(input);
        if (fi.isDir()) {
            const QStringList names =
                QDir(input).entryList(ImageStore::nameFilters(), QDir::Files, QDir::Name);
            for (const QString& name : names)
                files << QDir(input).absoluteFilePath(name);
        } else {
            files << input;
        }
    }
    return files;
}

int commandCompress(const QString& outPath, const QStringList& rawInputs)
{
    const QStringList inputs = expandInputs(rawInputs);
    if (inputs.isEmpty()) {
        err() << "error: -compress requires input files\n";
        return 2;
    }
    return Cli::pack(outPath, inputs);
}

int commandExtract(const QString& albumPath, const QString& dirArg)
{
    const QString dir = dirArg.isEmpty()
        ? QFileInfo(albumPath).absolutePath() + QLatin1Char('/')
            + QFileInfo(albumPath).completeBaseName()
        : dirArg;
    if (!QDir().mkpath(dir)) {
        err() << "error: cannot create directory: " << dir << Qt::endl;
        return 3;
    }

    Qtivp::EntryList entries;
    QString error;
    if (!Qtivp::Reader::openIndex(albumPath, entries, &error)) {
        err() << "error: " << error << Qt::endl;
        return 3;
    }

    int saved = 0;
    QStringList errors;
    for (const Qtivp::Entry& e : entries) {
        QString name = QFileInfo(e.name).fileName();
        if (name.isEmpty() || name == QLatin1String("."))
            name = QStringLiteral("image");
        QString target = QDir(dir).absoluteFilePath(name);
        // Не перезаписываем существующее — добавляем счётчик.
        if (QFileInfo::exists(target)) {
            const QString base = QFileInfo(name).completeBaseName();
            const QString suffix = QFileInfo(name).suffix();
            for (int i = 1; i < 10000; ++i) {
                const QString candidate = suffix.isEmpty()
                    ? QStringLiteral("%1-%2").arg(base).arg(i)
                    : QStringLiteral("%1-%2.%3").arg(base).arg(i).arg(suffix);
                target = QDir(dir).absoluteFilePath(candidate);
                if (!QFileInfo::exists(target))
                    break;
            }
        }

        QString blobError;
        const QByteArray blob = Qtivp::Reader::readBlob(albumPath, e, &blobError);
        if (blob.isEmpty()) {
            errors << QStringLiteral("%1: %2").arg(name, blobError);
            continue;
        }
        QFile f(target);
        if (!f.open(QIODevice::WriteOnly)) {
            errors << QStringLiteral("%1: %2").arg(name, f.errorString());
            continue;
        }
        if (f.write(blob) != blob.size()) {
            errors << QStringLiteral("%1: write error").arg(name);
            continue;
        }
        f.setPermissions(f.permissions() | QFileDevice::ReadUser | QFileDevice::WriteUser);
        ++saved;
    }

    out() << "OK: extracted " << saved << " photo(s) to " << QDir(dir).absolutePath()
          << Qt::endl;
    for (const QString& e : errors)
        err() << "warning: " << e << Qt::endl;
    return errors.isEmpty() ? 0 : (saved > 0 ? 0 : 4);
}

int commandList(const QString& albumPath)
{
    Qtivp::EntryList entries;
    QString error;
    if (!Qtivp::Reader::openIndex(albumPath, entries, &error)) {
        err() << "error: " << error << Qt::endl;
        return 3;
    }
    out() << albumPath << ": " << entries.size() << " photo(s), "
          << humanSize(QFileInfo(albumPath).size()) << Qt::endl;
    int i = 1;
    for (const Qtivp::Entry& e : entries) {
        QString dims = e.width > 0
            ? QStringLiteral("%1x%2").arg(e.width).arg(e.height)
            : QStringLiteral("?");
        out() << QStringLiteral("%1. %2  [%3]  %4  %5%6")
                     .arg(i++, 3)
                     .arg(e.name, -34)
                     .arg(e.mime)
                     .arg(dims, -11)
                     .arg(humanSize(e.rawSize))
                     .arg(e.flags & Qtivp::FlagZlib ? QStringLiteral("  (zlib)")
                                                    : QString())
              << Qt::endl;
    }
    return 0;
}

int commandTest(const QString& albumPath)
{
    Qtivp::EntryList entries;
    QString error;
    if (!Qtivp::Reader::openIndex(albumPath, entries, &error)) {
        err() << "error: " << error << Qt::endl;
        return 3;
    }
    int bad = 0;
    for (const Qtivp::Entry& e : entries) {
        QString blobError;
        const QByteArray blob = Qtivp::Reader::readBlob(albumPath, e, &blobError);
        if (blob.isEmpty()) {
            err() << "FAIL: " << e.name << ": " << blobError << Qt::endl;
            ++bad;
        } else {
            out() << "ok:   " << e.name << " (" << humanSize(e.rawSize) << ")" << Qt::endl;
        }
    }
    if (bad == 0) {
        out() << "OK: album is valid, " << entries.size() << " photo(s) verified" << Qt::endl;
        return 0;
    }
    err() << "FAILED: " << bad << " of " << entries.size() << " photo(s) are damaged" << Qt::endl;
    return 4;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qtivp"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QTIV_VERSION));

    if (argc < 2) {
        printUsage();
        return 0;
    }

    const QString command = QString::fromLocal8Bit(argv[1]);
    if (command == QLatin1String("-h") || command == QLatin1String("--help")) {
        printUsage();
        return 0;
    }
    if (command == QLatin1String("-v") || command == QLatin1String("--version")) {
        out() << "qtivp " << QTIV_VERSION << Qt::endl;
        return 0;
    }

    if (command == QLatin1String("-compress") || command == QLatin1String("--compress")
        || command == QLatin1String("c")) {
        if (argc < 4) {
            err() << "error: -compress requires an output file and input photos\n";
            return 2;
        }
        const QString outPath = QString::fromLocal8Bit(argv[2]);
        QStringList inputs;
        for (int i = 3; i < argc; ++i)
            inputs << QString::fromLocal8Bit(argv[i]);
        return commandCompress(outPath, inputs);
    }
    if (command == QLatin1String("-extract") || command == QLatin1String("--extract")
        || command == QLatin1String("x")) {
        if (argc < 3) {
            err() << "error: -extract requires an album file\n";
            return 2;
        }
        const QString album = QString::fromLocal8Bit(argv[2]);
        const QString dir = argc >= 4 ? QString::fromLocal8Bit(argv[3]) : QString();
        return commandExtract(album, dir);
    }
    if (command == QLatin1String("-list") || command == QLatin1String("--list")
        || command == QLatin1String("l")) {
        if (argc < 3) {
            err() << "error: -list requires an album file\n";
            return 2;
        }
        return commandList(QString::fromLocal8Bit(argv[2]));
    }
    if (command == QLatin1String("-test") || command == QLatin1String("--test")
        || command == QLatin1String("t")) {
        if (argc < 3) {
            err() << "error: -test requires an album file\n";
            return 2;
        }
        return commandTest(QString::fromLocal8Bit(argv[2]));
    }

    err() << "error: unknown command: " << command << "\n\n";
    printUsage();
    return 2;
}

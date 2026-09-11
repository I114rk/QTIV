#include <QtTest>

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QTemporaryDir>

#include "Crc32.h"
#include "Qtivp.h"

using namespace Qtivp;

namespace {
QImage makeImage(int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, qRgba((x * 255) / qMax(w - 1, 1), (y * 255) / qMax(h - 1, 1),
                                      (x + y) % 256, 255));
    return img;
}

QByteArray encodePng(const QImage& img)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, QByteArrayLiteral("png"));
    writer.write(img);
    return bytes;
}
} // namespace

class TestQtivp : public QObject {
    Q_OBJECT

private slots:
    void crc32KnownAnswer()
    {
        // Стандартное контрольное значение CRC-32.
        QCOMPARE(crc32(QByteArray("123456789")), quint32(0xCBF43926));
        QCOMPARE(crc32(QByteArray()), quint32(0x00000000));
    }

    void roundTrip()
    {
        const QImage a = makeImage(64, 48);
        const QImage b = a.flipped(Qt::Horizontal);
        const QByteArray pngA = encodePng(a);
        const QByteArray pngB = encodePng(b);

        EntryList entries;
        Entry e1;
        e1.name = QStringLiteral("first.png");
        e1.mime = QStringLiteral("image/png");
        e1.width = 64;
        e1.height = 48;
        e1.blob = pngA;
        entries.append(e1);

        Entry e2;
        e2.name = QStringLiteral("second.png");
        e2.mime = QStringLiteral("image/png");
        e2.width = 48;
        e2.height = 64;
        e2.blob = pngB;
        entries.append(e2);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("album.qtivp"));

        QString error;
        QVERIFY2(Writer::write(path, entries, &error), qPrintable(error));

        EntryList loaded;
        QVERIFY2(Reader::openIndex(path, loaded, &error), qPrintable(error));
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[0].name, QStringLiteral("first.png"));
        QCOMPARE(loaded[0].mime, QStringLiteral("image/png"));
        QCOMPARE(loaded[0].width, quint32(64));
        QCOMPARE(loaded[0].height, quint32(48));
        QCOMPARE(loaded[0].rawSize, quint32(pngA.size()));
        QCOMPARE(loaded[1].name, QStringLiteral("second.png"));

        // Blob'ы должны вернуться байт в байт.
        QCOMPARE(Reader::readBlob(path, loaded[0], &error), pngA);
        QCOMPARE(Reader::readBlob(path, loaded[1], &error), pngB);

        // Декодирование даёт то же изображение.
        const QImage decoded = Reader::decode(path, loaded[1], &error);
        QVERIFY2(!decoded.isNull(), qPrintable(error));
        QCOMPARE(decoded.size(), b.size());
    }

    void zlibCompression()
    {
        // Хорошо сжимаемые данные должны храниться с FlagZlib.
        Entry e;
        e.name = QStringLiteral("zeros.bin");
        e.mime = QStringLiteral("application/octet-stream");
        e.blob = QByteArray(200000, '\0');

        EntryList entries{ e };
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("album.qtivp"));

        QString error;
        QVERIFY2(Writer::write(path, entries, &error), qPrintable(error));

        EntryList loaded;
        QVERIFY2(Reader::openIndex(path, loaded, &error), qPrintable(error));
        QCOMPARE(loaded.size(), 1);
        QVERIFY(loaded[0].flags & FlagZlib);
        QVERIFY(loaded[0].storedSize < 2000); // 200 КБ нулей сжались сильно
        QCOMPARE(quint32(loaded[0].rawSize), quint32(e.blob.size()));

        const QByteArray restored = Reader::readBlob(path, loaded[0], &error);
        QVERIFY2(!restored.isEmpty(), qPrintable(error));
        QCOMPARE(restored.size(), e.blob.size());
    }

    void corruptedBlob()
    {
        EntryList entries;
        Entry e;
        e.name = QStringLiteral("photo.png");
        e.mime = QStringLiteral("image/png");
        const QImage img = makeImage(32, 32);
        e.width = 32;
        e.height = 32;
        e.blob = encodePng(img);
        entries.append(e);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("album.qtivp"));

        QString error;
        QVERIFY2(Writer::write(path, entries, &error), qPrintable(error));

        EntryList loaded;
        QVERIFY2(Reader::openIndex(path, loaded, &error), qPrintable(error));

        // Порча одного байта внутри blob-данных → ошибка CRC.
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.seek(qint64(loaded[0].offset) + loaded[0].storedSize / 2));
            f.putChar('\x5A');
            f.close();
        }
        QString blobError;
        const QByteArray bad = Reader::readBlob(path, loaded[0], &blobError);
        QVERIFY(bad.isEmpty());
        QVERIFY(!blobError.isEmpty());
    }

    void truncatedFile()
    {
        EntryList entries;
        for (int i = 0; i < 3; ++i) {
            Entry e;
            e.name = QStringLiteral("photo%1.png").arg(i);
            e.mime = QStringLiteral("image/png");
            const QImage img = makeImage(40, 30);
            e.width = 40;
            e.height = 30;
            e.blob = encodePng(img);
            entries.append(e);
        }

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("album.qtivp"));

        QString error;
        QVERIFY2(Writer::write(path, entries, &error), qPrintable(error));
        const qint64 fullSize = QFileInfo(path).size();

        EntryList loaded;
        QVERIFY2(Reader::openIndex(path, loaded, &error), qPrintable(error));
        QCOMPARE(loaded.size(), 3);

        // Обрезанный файл: индекс или blob нарушены → ошибка.
        QVERIFY(QFile(path).resize(fullSize - 100));
        EntryList truncated;
        QVERIFY(!Reader::openIndex(path, truncated, &error) || truncated.isEmpty()
                || Reader::readBlob(path, truncated.last(), &error).isEmpty());
    }

    void emptyAlbum()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("empty.qtivp"));
        QString error;
        QVERIFY2(Writer::write(path, {}, &error), qPrintable(error));

        EntryList loaded;
        QVERIFY2(Reader::openIndex(path, loaded, &error), qPrintable(error));
        QCOMPARE(loaded.size(), 0);
    }

    void notAnAlbum()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("fake.qtivp"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is definitely not a qtivp album");
        f.close();

        EntryList loaded;
        QString error;
        QVERIFY(!Reader::openIndex(path, loaded, &error));
        QVERIFY(!error.isEmpty());
    }

    void mimeHelpers()
    {
        QCOMPARE(mimeForFormat(QStringLiteral("png")), QStringLiteral("image/png"));
        QCOMPARE(mimeForFormat(QStringLiteral("JPG")), QStringLiteral("image/jpeg"));
        QCOMPARE(formatForMime(QStringLiteral("image/webp")), QStringLiteral("webp"));
        QCOMPARE(mimeForFormat(QStringLiteral("unknown")), QStringLiteral("application/octet-stream"));
    }
};

QTEST_GUILESS_MAIN(TestQtivp)
#include "tst_qtivp.moc"

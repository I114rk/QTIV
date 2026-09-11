#include "ImageStore.h"

#include <QBuffer>
#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QMimeDatabase>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace {
constexpr int kCacheMaxEntries = 16;
} // namespace

ImageStore::ImageStore(QObject* parent)
    : QObject(parent)
{
}

QStringList ImageStore::supportedExtensions()
{
    static const QStringList exts = [] {
        QSet<QString> set;
        const auto formats = QImageReader::supportedImageFormats();
        for (const QByteArray& f : formats)
            set.insert(QString::fromLatin1(f.toLower()));
        set << QStringLiteral("jpg") << QStringLiteral("jpeg"); // алиасы
        set << QStringLiteral("qtivp");
        QStringList list = set.values();
        list.sort();
        return list;
    }();
    return exts;
}

QStringList ImageStore::nameFilters()
{
    QStringList filters;
    for (const QString& ext : supportedExtensions())
        filters << QStringLiteral("*.") + ext;
    return filters;
}

bool ImageStore::isAlbumPath(const QString& path)
{
    return QFileInfo(path).suffix().compare(QStringLiteral("qtivp"), Qt::CaseInsensitive) == 0;
}

bool ImageStore::isSupportedPath(const QString& path)
{
    const QFileInfo fi(path);
    if (!fi.isFile())
        return false;
    const QString suffix = fi.suffix().toLower();
    return supportedExtensions().contains(suffix);
}

QString ImageStore::prettyFormat(const QString& format)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("jpg"),   QStringLiteral("JPEG")},
        {QStringLiteral("jpeg"),  QStringLiteral("JPEG")},
        {QStringLiteral("png"),   QStringLiteral("PNG")},
        {QStringLiteral("webp"),  QStringLiteral("WebP")},
        {QStringLiteral("avif"),  QStringLiteral("AVIF")},
        {QStringLiteral("bmp"),   QStringLiteral("BMP")},
        {QStringLiteral("gif"),   QStringLiteral("GIF")},
        {QStringLiteral("tiff"),  QStringLiteral("TIFF")},
        {QStringLiteral("tif"),   QStringLiteral("TIFF")},
        {QStringLiteral("heic"),  QStringLiteral("HEIC")},
    };
    const QString key = format.toLower();
    return map.value(key, key.toUpper());
}

QString ImageStore::openPaths(const QStringList& paths)
{
    m_cache.clear();
    m_albumIndexes.clear();

    QList<Item> items;
    QStringList errors;
    int current = -1;

    for (const QString& raw : paths) {
        QString path = raw;
        if (path.startsWith(QStringLiteral("file:")))
            path = QUrl(path).toLocalFile();
        const QFileInfo fi(path);
        if (!fi.exists()) {
            errors << QStringLiteral("%1: no such file or directory").arg(path);
            continue;
        }
        if (fi.isDir()) {
            const int first = items.size();
            appendFolder(fi.absoluteFilePath(), items, errors);
            if (items.size() > first && current < 0)
                current = first;
        } else if (isAlbumPath(path)) {
            const int first = items.size();
            appendAlbum(fi.absoluteFilePath(), items, errors);
            if (items.size() > first && current < 0)
                current = first;
        } else if (isSupportedPath(path)) {
            if (paths.size() == 1) {
                // QTVP-стиль: папка файла становится плейлистом, файл — в фокусе.
                const int first = items.size();
                appendFolder(fi.absolutePath(), items, errors);
                const QString abs = fi.absoluteFilePath();
                for (int i = first; i < items.size(); ++i) {
                    if (items[i].sourcePath == abs) {
                        current = i;
                        break;
                    }
                }
                if (current < 0) {
                    appendImageFile(abs, items, errors);
                    current = items.size() - 1;
                }
            } else {
                appendImageFile(fi.absoluteFilePath(), items, errors);
                if (current < 0)
                    current = items.size() - 1;
            }
        } else {
            errors << QStringLiteral("%1: unsupported file type").arg(path);
        }
    }

    if (items.isEmpty())
        return errors.isEmpty() ? QStringLiteral("nothing to open") : errors.join(u'\n');

    m_items = items;
    m_current = current < 0 ? 0 : current;
    emit listReset();
    emit currentChanged(m_current);
    return QString();
}

const ImageStore::Item& ImageStore::item(int index) const
{
    static const Item nullItem;
    if (index < 0 || index >= m_items.size())
        return nullItem;
    return m_items[index];
}

const ImageStore::Item* ImageStore::currentItem() const
{
    return m_current >= 0 && m_current < m_items.size() ? &m_items[m_current] : nullptr;
}

bool ImageStore::setCurrent(int index)
{
    if (index < 0 || index >= m_items.size())
        return false;
    if (index == m_current)
        return true;
    m_current = index;
    emit currentChanged(m_current);
    return true;
}

void ImageStore::next()
{
    if (m_items.isEmpty())
        return;
    setCurrent((m_current + 1) % m_items.size());
}

void ImageStore::previous()
{
    if (m_items.isEmpty())
        return;
    setCurrent((m_current - 1 + m_items.size()) % m_items.size());
}

void ImageStore::appendFolder(const QString& dirPath, QList<Item>& items, QStringList& errors)
{
    QDir dir(dirPath);
    QStringList names = dir.entryList(nameFilters(), QDir::Files, QDir::NoSort);

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(),
              [&collator](const QString& a, const QString& b) { return collator.compare(a, b) < 0; });

    for (const QString& name : std::as_const(names)) {
        const QString abs = dir.absoluteFilePath(name);
        if (isAlbumPath(abs))
            appendAlbum(abs, items, errors);
        else
            appendImageFile(abs, items, errors);
    }
}

void ImageStore::appendImageFile(const QString& filePath, QList<Item>& items, QStringList& errors)
{
    const QFileInfo fi(filePath);
    if (!fi.isFile()) {
        errors << QStringLiteral("%1: not a file").arg(filePath);
        return;
    }

    Item it;
    it.displayName = fi.fileName();
    it.sourcePath = fi.absoluteFilePath();
    it.albumIndex = -1;
    it.format = fi.suffix().toLower();

    QString mime = QMimeDatabase().mimeTypeForFile(fi.absoluteFilePath()).name();
    if (mime.startsWith(QStringLiteral("application/")))
        mime = Qtivp::mimeForFormat(it.format);
    it.mime = mime;
    it.byteSize = fi.size();

    // Размеры из заголовка файла (быстро), с поправкой на EXIF-поворот.
    QImageReader reader(it.sourcePath);
    QSize sz = reader.size();
    if (sz.isValid()) {
        if (reader.transformation() & QImageIOHandler::TransformationRotate90)
            sz.transpose();
        it.width = sz.width();
        it.height = sz.height();
    }
    items.append(it);
}

void ImageStore::appendAlbum(const QString& albumPath, QList<Item>& items, QStringList& errors)
{
    Qtivp::EntryList entries;
    QString err;
    if (!Qtivp::Reader::openIndex(albumPath, entries, &err)) {
        errors << err;
        return;
    }
    const QString abs = QFileInfo(albumPath).absoluteFilePath();
    m_albumIndexes.insert(abs, entries);

    for (int i = 0; i < entries.size(); ++i) {
        const Qtivp::Entry& e = entries[i];
        Item it;
        it.displayName = e.name;
        it.sourcePath = abs;
        it.albumIndex = i;
        it.format = QFileInfo(e.name).suffix().toLower();
        it.mime = e.mime.isEmpty() ? Qtivp::mimeForFormat(it.format) : e.mime;
        it.byteSize = qint64(e.rawSize);
        it.width = int(e.width);
        it.height = int(e.height);
        items.append(it);
    }
}

const Qtivp::EntryList* ImageStore::albumEntries(const QString& albumPath)
{
    const auto it = m_albumIndexes.constFind(albumPath);
    if (it != m_albumIndexes.constEnd())
        return &it.value();
    Qtivp::EntryList entries;
    if (!Qtivp::Reader::openIndex(albumPath, entries))
        return nullptr;
    return &m_albumIndexes.insert(albumPath, entries).value();
}

QImage ImageStore::image(int index)
{
    if (index < 0 || index >= m_items.size())
        return {};
    Cached& c = m_cache[index];
    c.lastUsed = ++m_cacheClock;

    if (!c.triedImage) {
        c.triedImage = true;
        const Item& it = m_items[index];
        if (it.fromAlbum()) {
            const Qtivp::EntryList* entries = albumEntries(it.sourcePath);
            if (entries && it.albumIndex < entries->size()) {
                QString err;
                c.image = Qtivp::Reader::decode(it.sourcePath, entries->at(it.albumIndex), &err);
                m_lastDecodeError = c.image.isNull() ? err : QString();
            } else {
                m_lastDecodeError = QStringLiteral("album entry not found: %1").arg(it.displayName);
            }
        } else {
            QImageReader reader(it.sourcePath);
            reader.setAutoTransform(true);
            c.image = reader.read();
            m_lastDecodeError = c.image.isNull() ? reader.errorString() : QString();
        }
        if (!c.image.isNull()) {
            m_items[index].width = c.image.width();
            m_items[index].height = c.image.height();
        }
        trimCache();
    }
    return c.image;
}

QByteArray ImageStore::encoded(int index)
{
    if (index < 0 || index >= m_items.size())
        return {};
    Cached& c = m_cache[index];
    c.lastUsed = ++m_cacheClock;

    if (!c.triedEncoded) {
        c.triedEncoded = true;
        const Item& it = m_items[index];
        if (it.fromAlbum()) {
            const Qtivp::EntryList* entries = albumEntries(it.sourcePath);
            if (entries && it.albumIndex < entries->size())
                c.encoded = Qtivp::Reader::readBlob(it.sourcePath, entries->at(it.albumIndex));
        } else {
            QFile f(it.sourcePath);
            if (f.open(QIODevice::ReadOnly))
                c.encoded = f.readAll();
        }
        trimCache();
    }
    return c.encoded;
}

QImage ImageStore::thumbnail(int index, const QSize& box)
{
    if (index < 0 || index >= m_items.size())
        return {};
    const Item& it = m_items[index];

    QImageReader reader;
    QBuffer buffer;
    if (it.fromAlbum()) {
        const Qtivp::EntryList* entries = albumEntries(it.sourcePath);
        if (!entries || it.albumIndex >= entries->size())
            return {};
        const QByteArray raw = Qtivp::Reader::readBlob(it.sourcePath, entries->at(it.albumIndex));
        if (raw.isEmpty())
            return {};
        buffer.setData(raw);
        buffer.open(QIODevice::ReadOnly);
        reader.setDevice(&buffer);
        reader.setDecideFormatFromContent(true);
    } else {
        reader.setFileName(it.sourcePath);
    }
    reader.setAutoTransform(true);

    const QSize sz = reader.size();
    if (sz.isValid()) {
        const QSize scaled = sz.scaled(box, Qt::KeepAspectRatio);
        if (!scaled.isEmpty())
            reader.setScaledSize(scaled);
    }
    QImage img = reader.read();
    if (!img.isNull() && !sz.isValid())
        img = img.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img;
}

void ImageStore::trimCache()
{
    while (m_cache.size() > kCacheMaxEntries) {
        auto oldest = m_cache.begin();
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
            if (it->lastUsed < oldest->lastUsed)
                oldest = it;
        m_cache.erase(oldest);
    }
}

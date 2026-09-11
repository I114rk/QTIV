#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QStringList>

#include "Qtivp.h"

// Плейлист изображений: файлы, папки и альбомы .qtivp.
// QImage и исходные байты декодируются лениво, кэшируются последние записи.
class ImageStore : public QObject {
    Q_OBJECT

public:
    struct Item {
        QString displayName;    // "IMG_2041.jpg" или имя записи альбома
        QString sourcePath;     // файл на диске (для альбома — путь к .qtivp)
        int albumIndex = -1;    // >= 0 → запись внутри альбома sourcePath
        QString format;         // "png", "jpeg", ...
        QString mime;           // "image/png", ...
        qint64 byteSize = 0;
        int width = 0;
        int height = 0;

        bool fromAlbum() const { return albumIndex >= 0; }
    };

    explicit ImageStore(QObject* parent = nullptr);

    static bool isAlbumPath(const QString& path);
    static bool isSupportedPath(const QString& path);
    static QStringList supportedExtensions();
    static QStringList nameFilters();                     // "*.png", ... + "*.qtivp"
    static QString prettyFormat(const QString& format);   // "jpeg" → "JPEG"

    // Открывает пути (файлы / папки / альбомы), заменяя плейлист.
    // Один файл → плейлист из его папки (стиль QTVP), с файлом в фокусе.
    // Возвращает пустую строку при успехе, иначе текст ошибки.
    QString openPaths(const QStringList& paths);

    int count() const { return m_items.size(); }
    bool isEmpty() const { return m_items.isEmpty(); }
    int current() const { return m_current; }
    bool setCurrent(int index);

    const Item& item(int index) const;
    const Item* currentItem() const;

    QImage image(int index);               // декодирует и кэширует
    QByteArray encoded(int index);         // исходные байты (для экспорта/упаковки)
    QImage thumbnail(int index, const QSize& box);
    QString lastDecodeError() const { return m_lastDecodeError; }

    void next();
    void previous();

signals:
    void listReset();
    void currentChanged(int index);

private:
    struct Cached {
        QImage image;
        QByteArray encoded;
        bool triedImage = false;
        bool triedEncoded = false;
        quint64 lastUsed = 0;
    };

    void appendFolder(const QString& dirPath, QList<Item>& items, QStringList& errors);
    void appendImageFile(const QString& filePath, QList<Item>& items, QStringList& errors);
    void appendAlbum(const QString& albumPath, QList<Item>& items, QStringList& errors);
    const Qtivp::EntryList* albumEntries(const QString& albumPath);
    void trimCache();

    QList<Item> m_items;
    int m_current = -1;
    QHash<int, Cached> m_cache;
    QHash<QString, Qtivp::EntryList> m_albumIndexes;
    quint64 m_cacheClock = 0;
    QString m_lastDecodeError;
};

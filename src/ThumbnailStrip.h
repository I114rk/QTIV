#pragma once

#include <QList>
#include <QPixmap>
#include <QWidget>

class ImageStore;
class QListWidget;
class QListWidgetItem;
class QTimer;

// Горизонтальная лента миниатюр внизу главного окна.
// Миниатюры генерируются в фоне (по таймеру) и не блокируют UI.
class ThumbnailStrip : public QWidget {
    Q_OBJECT

public:
    explicit ThumbnailStrip(ImageStore* store, QWidget* parent = nullptr);

    void setCurrentIndex(int index);
    void retranslateUi();

signals:
    void indexActivated(int index);

private slots:
    void onItemClicked(QListWidgetItem* item);
    void generateNextThumbnail();

private:
    void rebuild();

    ImageStore* m_store;
    QListWidget* m_list = nullptr;
    QTimer* m_timer = nullptr;
    int m_nextThumb = 0;      // следующая миниатюра для фоновой генерации
    int m_thumbSize = 96;
};

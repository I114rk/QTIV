#pragma once

#include <QList>
#include <QPixmap>
#include <QWidget>

class ImageStore;
class QListWidget;
class QListWidgetItem;
class QTimer;
class QEvent;

// Горизонтальная лента миниатюр внизу главного окна.
// Миниатюры генерируются в фоне (по таймеру) и не блокируют UI.
class ThumbnailStrip : public QWidget {
    Q_OBJECT

public:
    explicit ThumbnailStrip(ImageStore* store, QWidget* parent = nullptr);

    // Роль данных пункта ленты: элемент открывает новую группу.
    // Группа = файл-источник: записи одного альбома .qtivp — одна группа,
    // обычные фото — каждая своя (разделитель между каждыми).
    static constexpr int kGroupStartRole = Qt::UserRole + 1;

    void setCurrentIndex(int index);
    void retranslateUi();

signals:
    void indexActivated(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

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

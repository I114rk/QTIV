#include "ThumbnailStrip.h"
#include "ImageStore.h"

#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QPixmap placeholderThumb(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setPen(QPen(QColor(0x55, 0x5b, 0x69), 2));
    const int m = size / 4;
    p.drawRect(m, m, size - 2 * m, size - 2 * m);
    p.drawLine(m, m, size - m, size - m);
    p.drawLine(m, size - m, size - m, m);
    return pm;
}

} // namespace

ThumbnailStrip::ThumbnailStrip(ImageStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(false);
    m_list->setMovement(QListView::Static);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setFixedHeight(m_thumbSize + 22);
    m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_list);

    m_timer = new QTimer(this);
    m_timer->setInterval(15);
    connect(m_timer, &QTimer::timeout, this, &ThumbnailStrip::generateNextThumbnail);

    connect(store, &ImageStore::listReset, this, &ThumbnailStrip::rebuild);
    connect(store, &ImageStore::currentChanged, this, &ThumbnailStrip::setCurrentIndex);
    connect(m_list, &QListWidget::itemClicked, this, &ThumbnailStrip::onItemClicked);
    // Навигация стрелками внутри ленты (currentRowChanged) — так же, как кликом;
    // циклов нет: программная установка в setCurrentIndex идёт под blockSignals.
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0)
            emit indexActivated(row);
    });

    rebuild();
}

void ThumbnailStrip::rebuild()
{
    m_list->clear();
    m_nextThumb = 0;
    const int n = m_store->count();
    if (n == 0) {
        m_timer->stop();
        return;
    }

    const QSize thumb(m_thumbSize, m_thumbSize);
    for (int i = 0; i < n; ++i) {
        const QString name = m_store->item(i).displayName;
        auto* item = new QListWidgetItem(m_list);
        item->setText(name);
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(thumb + QSize(12, 18));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setData(Qt::UserRole, i);
    }
    setCurrentIndex(m_store->current());
    m_timer->start();
}

void ThumbnailStrip::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_list->count())
        return;
    m_list->blockSignals(true);
    m_list->setCurrentRow(index);
    m_list->blockSignals(false);
    m_list->scrollToItem(m_list->item(index), QAbstractItemView::PositionAtCenter);
}

void ThumbnailStrip::onItemClicked(QListWidgetItem* item)
{
    const int index = item->data(Qt::UserRole).toInt();
    emit indexActivated(index);
}

void ThumbnailStrip::generateNextThumbnail()
{
    const int n = m_store->count();
    if (m_nextThumb >= n) {
        m_timer->stop();
        return;
    }

    const int idx = m_nextThumb++;
    const QImage img = m_store->thumbnail(idx, QSize(m_thumbSize, m_thumbSize));
    if (QListWidgetItem* item = m_list->item(idx)) {
        if (img.isNull())
            item->setIcon(placeholderThumb(m_thumbSize));
        else
            item->setIcon(QPixmap::fromImage(img));
    }
    if (m_nextThumb >= n)
        m_timer->stop();
}

void ThumbnailStrip::retranslateUi()
{
    // Тексты внутри пунктов — имена файлов, локализации не требуют.
}

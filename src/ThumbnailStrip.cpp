#include "ThumbnailStrip.h"
#include "ImageStore.h"

#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {

// Цвет линий-разделителей и плейсхолдеров — общий для всей ленты.
QColor lineColor()
{
    return {0x55, 0x5b, 0x69};
}

QPixmap placeholderThumb(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setPen(QPen(lineColor(), 2));
    const int m = size / 4;
    p.drawRect(m, m, size - 2 * m, size - 2 * m);
    p.drawLine(m, m, size - m, size - m);
    p.drawLine(m, size - m, size - m, m);
    return pm;
}

// Рисует вертикальный разделитель перед миниатюрами, начинающими новую
// группу (новый файл-источник). Записи одного альбома .qtivp — единая
// группа и не разделяются между собой.
class GroupDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        if (index.row() == 0 || !index.data(ThumbnailStrip::kGroupStartRole).toBool())
            return;
        const QRect r = option.rect;
        painter->save();
        painter->setPen(QPen(lineColor(), 1));
        painter->drawLine(r.left() + 1, r.top() + 6, r.left() + 1, r.bottom() - 6);
        painter->restore();
    }
};

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

    m_list->setItemDelegate(new GroupDelegate(m_list));
    m_list->viewport()->installEventFilter(this);

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
        const ImageStore::Item& it = m_store->item(i);
        // Группа = файл-источник: записи одного альбома идут без разделителей.
        const bool groupStart = i == 0 || it.sourcePath != m_store->item(i - 1).sourcePath;
        auto* item = new QListWidgetItem(m_list);
        item->setText(it.displayName);
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(thumb + QSize(12, 18));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setData(Qt::UserRole, i);
        item->setData(kGroupStartRole, groupStart);
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

bool ThumbnailStrip::eventFilter(QObject* watched, QEvent* event)
{
    // Колесо мыши над лентой прокручивает миниатюры по горизонтали,
    // когда они не помещаются в видимое поле. Наклон колеса (angleDelta().x())
    // скроллит напрямую, обычное вращение переводится в горизонталь.
    if (watched == m_list->viewport() && event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        QScrollBar* bar = m_list->horizontalScrollBar();
        if (bar->maximum() > bar->minimum()) {
            const QPoint angles = wheel->angleDelta();
            const int delta = qAbs(angles.x()) > qAbs(angles.y()) ? angles.x() : angles.y();
            if (delta != 0) {
                wheel->accept();
                bar->setValue(bar->value() - delta);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

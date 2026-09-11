#include "CompareView.h"
#include "ImageStore.h"
#include "Locale.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWheelEvent>

CompareView::CompareView(ImageStore* store, int panels, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    setAutoFillBackground(true);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // --- Тулбар: комбобоксы панелей + кнопки ---------------------------
    m_barLayout = new QHBoxLayout;
    m_barLayout->addStretch(1);

    m_syncButton = new QPushButton(this);
    m_syncButton->setCheckable(true);
    m_syncButton->setChecked(m_syncEnabled);
    connect(m_syncButton, &QPushButton::toggled, this, &CompareView::setSyncEnabled);

    m_addButton = new QPushButton(this);
    connect(m_addButton, &QPushButton::clicked, this,
            [this] { setPanelCount(m_panels.size() + 1); });

    m_removeButton = new QPushButton(this);
    connect(m_removeButton, &QPushButton::clicked, this,
            [this] { setPanelCount(m_panels.size() - 1); });

    m_closeButton = new QPushButton(this);
    connect(m_closeButton, &QPushButton::clicked, this, &CompareView::closed);

    m_barLayout->addWidget(m_syncButton);
    m_barLayout->addSpacing(6);
    m_barLayout->addWidget(m_addButton);
    m_barLayout->addSpacing(6);
    m_barLayout->addWidget(m_removeButton);
    m_barLayout->addSpacing(12);
    m_barLayout->addWidget(m_closeButton);
    rootLayout->addLayout(m_barLayout);

    // --- Панели ---------------------------------------------------------
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    rootLayout->addWidget(m_splitter, 1);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignHCenter);
    rootLayout->addWidget(m_infoLabel);

    setPanelCount(panels);
    updateSyncButtons();
    retranslateUi();
}

int CompareView::panelIndex(int panel) const
{
    if (panel < 0 || panel >= m_panels.size())
        return -1;
    return m_panels[panel].imageIndex;
}

void CompareView::setPanelIndex(int panel, int index)
{
    if (panel < 0 || panel >= m_panels.size())
        return;
    Panel& p = m_panels[panel];
    if (index < 0 || index >= m_store->count() || index == p.imageIndex)
        return;
    p.imageIndex = index;
    if (p.combo->currentData().toInt() != index)
        populateCombo(p);
    updatePanel(p);
    if (m_syncEnabled && !m_panels.isEmpty())
        applySyncFrom(m_panels.first().view);
}

void CompareView::setIndices(const QVector<int>& indices)
{
    if (indices.isEmpty())
        return;
    setPanelCount(indices.size());
    for (int i = 0; i < indices.size() && i < m_panels.size(); ++i)
        setPanelIndex(i, indices[i]);
    if (m_syncEnabled && !m_panels.isEmpty())
        applySyncFrom(m_panels.first().view);
}

void CompareView::setPanelCount(int count)
{
    const int n = m_store->count();
    count = qMax(2, count);
    if (n > 1)
        count = qMin(count, n);
    count = qMin(count, kMaxPanelCount);

    while (m_panels.size() < count) {
        Panel p;
        p.imageIndex = defaultImageIndex();
        p.view = new ImageView(this);
        p.combo = new QComboBox(this);
        p.combo->setMinimumWidth(150);
        p.combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        p.combo->installEventFilter(this);
        p.view->setSyncEnabled(m_syncEnabled);

        connect(p.combo, &QComboBox::currentIndexChanged, this,
                [this, combo = p.combo](int row) {
                    for (int i = 0; i < m_panels.size(); ++i) {
                        if (m_panels[i].combo == combo) {
                            setPanelIndex(i, combo->itemData(row).toInt());
                            break;
                        }
                    }
                });
        connect(p.view, &ImageView::syncChanged, this,
                [this, source = p.view](const ImageView::SyncState& s) {
                    if (m_syncEnabled)
                        applySyncToOthers(source, s);
                });
        connect(p.view, &ImageView::zoomChanged, this, [this] { updateInfo(); });

        m_splitter->addWidget(p.view);
        m_barLayout->insertWidget(m_panels.size(), p.combo, 1);
        m_panels.append(p);
    }
    while (m_panels.size() > count) {
        const Panel p = m_panels.takeLast();
        p.combo->deleteLater();
        p.view->deleteLater();
    }

    for (int i = 0; i < m_splitter->count(); ++i)
        m_splitter->setStretchFactor(i, 1);

    m_addButton->setEnabled(m_panels.size() < kMaxPanelCount && m_panels.size() < n);
    m_removeButton->setEnabled(m_panels.size() > 2);
    for (Panel& p : m_panels) {
        populateCombo(p);
        updatePanel(p);
    }
    updateInfo();
}

int CompareView::defaultImageIndex() const
{
    // Первое фото плейлиста, которого ещё нет ни на одной панели.
    const int n = m_store->count();
    if (n <= 0)
        return -1;
    const int start = qMax(0, m_store->current()) + m_panels.size();
    for (int step = 0; step < n; ++step) {
        const int candidate = (start + step) % n;
        bool used = false;
        for (const Panel& p : m_panels) {
            if (p.imageIndex == candidate) {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
    }
    return start % n;
}

void CompareView::setSyncEnabled(bool on)
{
    m_syncEnabled = on;
    for (Panel& p : m_panels)
        p.view->setSyncEnabled(on);
    m_syncButton->setChecked(on);
    if (on && !m_panels.isEmpty())
        applySyncFrom(m_panels.first().view);
    updateSyncButtons();
}

void CompareView::retranslateUi()
{
    m_addButton->setText(i18n::s("+ panel"));
    m_addButton->setToolTip(i18n::s("Add a comparison panel"));
    m_removeButton->setText(i18n::s("\xE2\x88\x92 panel"));
    m_removeButton->setToolTip(i18n::s("Remove the last comparison panel"));
    m_closeButton->setText(i18n::s("Close"));
    m_syncButton->setToolTip(i18n::s("Synchronize zoom and panning between panels (S)"));
    for (Panel& p : m_panels)
        p.combo->setToolTip(i18n::s("Image for this panel"));
    updateSyncButtons();
    for (Panel& p : m_panels)
        updatePanel(p);
    updateInfo();
}

void CompareView::updateSyncButtons()
{
    m_syncButton->setText(m_syncEnabled ? i18n::s("Sync: on") : i18n::s("Sync: off"));
}

void CompareView::populateCombo(Panel& panel)
{
    panel.combo->blockSignals(true);
    panel.combo->clear();
    for (int i = 0; i < m_store->count(); ++i) {
        const ImageStore::Item& it = m_store->item(i);
        panel.combo->addItem(QStringLiteral("%1 — %2").arg(i + 1).arg(it.displayName), i);
    }
    const int pos = panel.combo->findData(panel.imageIndex);
    if (pos >= 0)
        panel.combo->setCurrentIndex(pos);
    panel.combo->blockSignals(false);
}

void CompareView::updatePanel(Panel& panel)
{
    if (panel.imageIndex < 0 || panel.imageIndex >= m_store->count()) {
        panel.view->clear();
        panel.view->setBlankText(i18n::s("Nothing to show"));
        updateInfo();
        return;
    }
    const QImage img = m_store->image(panel.imageIndex);
    if (img.isNull()) {
        panel.view->clear();
        panel.view->setBlankText(i18n::s("Image could not be decoded"));
    } else {
        panel.view->setImage(img);
    }
    updateInfo();
}

void CompareView::updateInfo()
{
    QStringList parts;
    for (const Panel& p : m_panels) {
        if (p.imageIndex < 0 || p.imageIndex >= m_store->count()) {
            parts << i18n::s("—");
            continue;
        }
        const ImageStore::Item& it = m_store->item(p.imageIndex);
        const int zoom = qRound(p.view->scale() * 100.0);
        parts << QStringLiteral("%1  ·  %2×%3  ·  %4%")
                     .arg(it.displayName)
                     .arg(it.width)
                     .arg(it.height)
                     .arg(zoom);
    }
    m_infoLabel->setText(parts.join(QStringLiteral("   |   ")));
}

void CompareView::applySyncFrom(ImageView* source)
{
    if (!source)
        return;
    applySyncToOthers(source, source->syncState());
}

void CompareView::applySyncToOthers(ImageView* source, const ImageView::SyncState& state)
{
    for (Panel& p : m_panels) {
        if (p.view != source)
            p.view->applySync(state);
    }
}

void CompareView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        event->accept();
        emit closed();
        return;
    case Qt::Key_S:
        if (event->modifiers() == Qt::NoModifier) {
            setSyncEnabled(!m_syncEnabled);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        for (Panel& p : m_panels)
            p.view->zoomIn();
        event->accept();
        return;
    case Qt::Key_Minus:
        for (Panel& p : m_panels)
            p.view->zoomOut();
        event->accept();
        return;
    case Qt::Key_0:
        for (Panel& p : m_panels)
            p.view->fitToWindow();
        event->accept();
        return;
    case Qt::Key_1:
        for (Panel& p : m_panels)
            p.view->actualSize();
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

bool CompareView::eventFilter(QObject* obj, QEvent* event)
{
    // Колесо над комбобоксами не должно случайно менять выбранное изображение.
    if (event->type() == QEvent::Wheel) {
        for (const Panel& p : m_panels) {
            if (obj == p.combo) {
                event->ignore();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

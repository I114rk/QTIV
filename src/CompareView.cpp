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

CompareView::CompareView(ImageStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    setAutoFillBackground(true);

    const int current = store->current();
    const int count = store->count();
    m_leftIndex = current >= 0 ? current : (count > 0 ? 0 : -1);
    m_rightIndex = count > 1 ? (current + 1) % count : m_leftIndex;

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // --- Тулбар выбора изображений -------------------------------------
    auto* bar = new QHBoxLayout;
    m_leftCombo = new QComboBox(this);
    m_leftCombo->setMinimumWidth(200);
    m_leftCombo->setToolTip(i18n::s("Image on the left (A)"));
    m_leftCombo->installEventFilter(this);
    m_rightCombo = new QComboBox(this);
    m_rightCombo->setMinimumWidth(200);
    m_rightCombo->setToolTip(i18n::s("Image on the right (D)"));
    m_rightCombo->installEventFilter(this);

    m_syncButton = new QPushButton(this);
    m_syncButton->setCheckable(true);
    m_syncButton->setChecked(m_syncEnabled);
    connect(m_syncButton, &QPushButton::toggled, this, &CompareView::setSyncEnabled);

    m_closeButton = new QPushButton(this);
    connect(m_closeButton, &QPushButton::clicked, this, &CompareView::closed);

    bar->addWidget(m_leftCombo, 1);
    bar->addSpacing(6);
    bar->addWidget(m_syncButton);
    bar->addSpacing(6);
    bar->addWidget(m_rightCombo, 1);
    bar->addSpacing(6);
    bar->addWidget(m_closeButton);
    rootLayout->addLayout(bar);

    // --- Панели ---------------------------------------------------------
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    m_left = new ImageView(this);
    m_right = new ImageView(this);
    m_left->setSyncEnabled(m_syncEnabled);
    m_right->setSyncEnabled(m_syncEnabled);
    splitter->addWidget(m_left);
    splitter->addWidget(m_right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);
    rootLayout->addWidget(splitter, 1);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignHCenter);
    rootLayout->addWidget(m_infoLabel);

    populateCombo(m_leftCombo, m_leftIndex);
    populateCombo(m_rightCombo, m_rightIndex);

    connect(m_leftCombo, &QComboBox::currentIndexChanged, this, [this](int i) {
        setLeftIndex(m_leftCombo->itemData(i).toInt());
    });
    connect(m_rightCombo, &QComboBox::currentIndexChanged, this, [this](int i) {
        setRightIndex(m_rightCombo->itemData(i).toInt());
    });

    connect(m_left, &ImageView::syncChanged, this, [this](const ImageView::SyncState& s) {
        if (m_syncEnabled)
            m_right->applySync(s);
    });
    connect(m_right, &ImageView::syncChanged, this, [this](const ImageView::SyncState& s) {
        if (m_syncEnabled)
            m_left->applySync(s);
    });
    connect(m_left, &ImageView::zoomChanged, this, [this] { updateInfo(); });
    connect(m_right, &ImageView::zoomChanged, this, [this] { updateInfo(); });

    updatePanel(0);
    updatePanel(1);
    updateInfo();
    updateSyncButtons();
    retranslateUi();
}

void CompareView::setLeftIndex(int index)
{
    if (index < 0 || index >= m_store->count() || index == m_leftIndex)
        return;
    m_leftIndex = index;
    if (m_leftCombo->currentData().toInt() != index)
        populateCombo(m_leftCombo, index);
    updatePanel(0);
    if (m_syncEnabled)
        applySyncFrom(m_right);
}

void CompareView::setRightIndex(int index)
{
    if (index < 0 || index >= m_store->count() || index == m_rightIndex)
        return;
    m_rightIndex = index;
    if (m_rightCombo->currentData().toInt() != index)
        populateCombo(m_rightCombo, index);
    updatePanel(1);
    if (m_syncEnabled)
        applySyncFrom(m_right);
}

void CompareView::setSyncEnabled(bool on)
{
    m_syncEnabled = on;
    m_left->setSyncEnabled(on);
    m_right->setSyncEnabled(on);
    m_syncButton->setChecked(on);
    if (on)
        applySyncFrom(m_left);
    updateSyncButtons();
}

void CompareView::retranslateUi()
{
    m_leftCombo->setToolTip(i18n::s("Image on the left (A)"));
    m_rightCombo->setToolTip(i18n::s("Image on the right (D)"));
    m_closeButton->setText(i18n::s("Close"));
    updateSyncButtons();
    m_left->setBlankText(i18n::s("Image could not be decoded"));
    m_right->setBlankText(i18n::s("Image could not be decoded"));
    if (m_leftIndex >= 0)
        updatePanel(0);
    if (m_rightIndex >= 0)
        updatePanel(1);
    updateInfo();
}

void CompareView::updateSyncButtons()
{
    m_syncButton->setText(m_syncEnabled ? i18n::s("Sync: on") : i18n::s("Sync: off"));
    m_syncButton->setToolTip(i18n::s("Synchronize zoom and panning between panels (S)"));
}

void CompareView::populateCombo(QComboBox* combo, int currentIndex)
{
    combo->blockSignals(true);
    combo->clear();
    for (int i = 0; i < m_store->count(); ++i) {
        const ImageStore::Item& it = m_store->item(i);
        combo->addItem(QStringLiteral("%1 — %2").arg(i + 1).arg(it.displayName), i);
    }
    const int pos = combo->findData(currentIndex);
    if (pos >= 0)
        combo->setCurrentIndex(pos);
    combo->blockSignals(false);
}

void CompareView::updatePanel(int side)
{
    const int index = side == 0 ? m_leftIndex : m_rightIndex;
    ImageView* view = side == 0 ? m_left : m_right;
    if (index < 0 || index >= m_store->count()) {
        view->clear();
        view->setBlankText(i18n::s("Nothing to show"));
        return;
    }
    const QImage img = m_store->image(index);
    if (img.isNull()) {
        view->clear();
        view->setBlankText(i18n::s("Image could not be decoded"));
    } else {
        view->setImage(img);
    }
    updateInfo();
}

void CompareView::updateInfo()
{
    auto describe = [this](int index, ImageView* view) {
        if (index < 0 || index >= m_store->count())
            return i18n::s("—");
        const ImageStore::Item& it = m_store->item(index);
        const int zoom = qRound(view->scale() * 100.0);
        return QStringLiteral("%1  ·  %2×%3  ·  %4%")
            .arg(it.displayName)
            .arg(it.width)
            .arg(it.height)
            .arg(zoom);
    };
    m_infoLabel->setText(QStringLiteral("%1   |   %2")
                             .arg(describe(m_leftIndex, m_left), describe(m_rightIndex, m_right)));
}

void CompareView::applySyncFrom(ImageView* source)
{
    if (source == m_left)
        m_right->applySync(m_left->syncState());
    else
        m_left->applySync(m_right->syncState());
}

void CompareView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        event->accept();
        emit closed();
        return;
    case Qt::Key_A:
        if (event->modifiers() == Qt::NoModifier) {
            m_leftCombo->showPopup();
            event->accept();
            return;
        }
        break;
    case Qt::Key_D:
        if (event->modifiers() == Qt::NoModifier) {
            m_rightCombo->showPopup();
            event->accept();
            return;
        }
        break;
    case Qt::Key_S:
        if (event->modifiers() == Qt::NoModifier) {
            setSyncEnabled(!m_syncEnabled);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        m_left->zoomIn();
        m_right->zoomIn();
        event->accept();
        return;
    case Qt::Key_Minus:
        m_left->zoomOut();
        m_right->zoomOut();
        event->accept();
        return;
    case Qt::Key_0:
        m_left->fitToWindow();
        m_right->fitToWindow();
        event->accept();
        return;
    case Qt::Key_1:
        m_left->actualSize();
        m_right->actualSize();
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
    if ((obj == m_leftCombo || obj == m_rightCombo) && event->type() == QEvent::Wheel) {
        event->ignore();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

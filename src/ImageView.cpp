#include "ImageView.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTextOption>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMaxScale = 64.0;
constexpr double kZoomStep = 1.25;

const QPixmap& checkerPixmap()
{
    static QPixmap pm;
    if (pm.isNull()) {
        constexpr int s = 14;
        pm = QPixmap(s * 2, s * 2);
        pm.fill(QColor(0x2a, 0x2d, 0x33));
        QPainter p(&pm);
        p.fillRect(0, 0, s, s, QColor(0x23, 0x25, 0x2a));
        p.fillRect(s, s, s, s, QColor(0x23, 0x25, 0x2a));
    }
    return pm;
}

} // namespace

ImageView::ImageView(QWidget* parent)
    : QWidget(parent)
{
}

void ImageView::setImage(const QImage& image)
{
    m_image = image;
    m_dragging = false;
    m_fit = true;
    updateFit();
    setCursor(m_image.isNull() ? Qt::ArrowCursor : Qt::OpenHandCursor);
    update();
    emit zoomChanged(m_scale);
    emit fitModeChanged(m_fit);
}

void ImageView::clear()
{
    m_image = {};
    m_fit = true;
    m_scale = 1.0;
    m_pan = {0, 0};
    m_dragging = false;
    setCursor(Qt::ArrowCursor);
    update();
}

void ImageView::setBlankText(const QString& text)
{
    m_blankText = text;
    update();
}

void ImageView::fitToWindow()
{
    if (m_image.isNull())
        return;
    m_fit = true;
    updateFit();
    update();
    notifyInteraction();
}

void ImageView::actualSize()
{
    if (m_image.isNull())
        return;
    zoomAt(QPointF(width() / 2.0, height() / 2.0), 1.0);
}

void ImageView::zoomIn()
{
    if (m_image.isNull())
        return;
    zoomAt(QPointF(width() / 2.0, height() / 2.0), m_scale * kZoomStep);
}

void ImageView::zoomOut()
{
    if (m_image.isNull())
        return;
    zoomAt(QPointF(width() / 2.0, height() / 2.0), m_scale / kZoomStep);
}

ImageView::SyncState ImageView::syncState() const
{
    SyncState st;
    st.scale = m_scale;
    if (!m_image.isNull()) {
        const QPointF c = screenToImg(QPointF(width() / 2.0, height() / 2.0));
        st.uv = QPointF(c.x() / m_image.width(), c.y() / m_image.height());
    }
    return st;
}

void ImageView::applySync(const SyncState& state)
{
    if (m_image.isNull())
        return;
    m_applyingSync = true;
    m_fit = false;
    m_scale = std::clamp(state.scale, minScale(), kMaxScale);
    const QPointF imgPt(state.uv.x() * m_image.width(), state.uv.y() * m_image.height());
    m_pan = QPointF(width() / 2.0, height() / 2.0) - imgPt * m_scale;
    clampPan();
    update();
    emit zoomChanged(m_scale);
    emit fitModeChanged(m_fit);
    m_applyingSync = false;
}

void ImageView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x14, 0x16, 0x1a));

    if (m_image.isNull()) {
        if (!m_blankText.isEmpty()) {
            p.setPen(QColor(0x8a, 0x90, 0x9e));
            QFont f = p.font();
            if (f.pointSizeF() > 0)
                f.setPointSizeF(f.pointSizeF() * 1.25);
            else if (f.pixelSize() > 0)
                f.setPixelSize(int(f.pixelSize() * 1.25));
            p.setFont(f);
            QTextOption to;
            to.setWrapMode(QTextOption::WordWrap);
            to.setAlignment(Qt::AlignCenter);
            p.drawText(QRect(24, 24, width() - 48, height() - 48), m_blankText, to);
        }
        return;
    }

    const QRectF target(m_pan, QSizeF(m_image.width() * m_scale, m_image.height() * m_scale));
    if (m_image.hasAlphaChannel()) {
        p.save();
        p.setClipRect(target.toAlignedRect());
        p.drawTiledPixmap(target.toRect(), checkerPixmap());
        p.restore();
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_scale < 1.0);
    p.drawImage(target, m_image);
}

void ImageView::wheelEvent(QWheelEvent* e)
{
    if (m_image.isNull()) {
        e->ignore();
        return;
    }
    e->accept();

    if (e->angleDelta().x() != 0) {
        m_fit = false;
        m_pan.rx() -= e->angleDelta().x() / 2.0;
        clampPan();
        update();
        notifyInteraction();
    }
    if (e->angleDelta().y() != 0) {
        const double factor = std::pow(1.0018, e->angleDelta().y());
        zoomAt(e->position(), m_scale * factor);
    }
}

void ImageView::mousePressEvent(QMouseEvent* e)
{
    if (m_image.isNull()
        || (e->button() != Qt::LeftButton && e->button() != Qt::MiddleButton)
        || (m_fit && !imageOverflows())) {
        e->ignore();
        return;
    }
    e->accept();
    m_dragging = true;
    m_pressPos = e->pos();
    m_panAtPress = m_pan;
    setCursor(Qt::ClosedHandCursor);
}

void ImageView::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) {
        e->ignore();
        return;
    }
    e->accept();
    m_fit = false;
    m_pan = m_panAtPress + QPointF(e->pos() - m_pressPos);
    clampPan();
    update();
    notifyInteraction();
}

void ImageView::mouseReleaseEvent(QMouseEvent* e)
{
    if (!m_dragging) {
        e->ignore();
        return;
    }
    e->accept();
    m_dragging = false;
    setCursor(m_image.isNull() ? Qt::ArrowCursor : Qt::OpenHandCursor);
}

void ImageView::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (m_image.isNull()) {
        e->ignore();
        return;
    }
    e->accept();
    if (m_fit) {
        zoomAt(e->position(), 1.0);
    } else {
        m_fit = true;
        updateFit();
        update();
        notifyInteraction();
    }
}

void ImageView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (m_fit) {
        updateFit();
        update();
    }
}

void ImageView::updateFit()
{
    if (m_image.isNull() || width() <= 0 || height() <= 0) {
        m_scale = 1.0;
        m_pan = {0, 0};
        return;
    }
    m_scale = std::clamp(fitScale() * 0.98, minScale(), kMaxScale);
    centerImage();
}

void ImageView::centerImage()
{
    m_pan = QPointF((width() - m_image.width() * m_scale) / 2.0,
                    (height() - m_image.height() * m_scale) / 2.0);
}

void ImageView::clampPan()
{
    if (m_image.isNull())
        return;
    const double sw = m_image.width() * m_scale;
    const double sh = m_image.height() * m_scale;
    const double vw = width();
    const double vh = height();
    if (sw <= vw)
        m_pan.setX((vw - sw) / 2.0);
    else
        m_pan.setX(std::clamp(m_pan.x(), vw - sw, 0.0));
    if (sh <= vh)
        m_pan.setY((vh - sh) / 2.0);
    else
        m_pan.setY(std::clamp(m_pan.y(), vh - sh, 0.0));
}

void ImageView::zoomAt(const QPointF& screenPos, double newScale)
{
    if (m_image.isNull())
        return;
    newScale = std::clamp(newScale, minScale(), kMaxScale);
    const QPointF imgPt = screenToImg(screenPos);
    m_scale = newScale;
    m_fit = false;
    m_pan = screenPos - imgPt * m_scale;
    clampPan();
    update();
    notifyInteraction();
}

void ImageView::notifyInteraction()
{
    emit zoomChanged(m_scale);
    emit fitModeChanged(m_fit);
    if (m_syncEnabled && !m_applyingSync)
        emit syncChanged(syncState());
}

bool ImageView::imageOverflows() const
{
    if (m_image.isNull())
        return false;
    return m_image.width() * m_scale > double(width()) + 0.5
        || m_image.height() * m_scale > double(height()) + 0.5;
}

double ImageView::fitScale() const
{
    if (m_image.isNull() || width() <= 0 || height() <= 0)
        return 1.0;
    return std::min(double(width()) / m_image.width(), double(height()) / m_image.height());
}

double ImageView::minScale() const
{
    if (m_image.isNull())
        return 0.01;
    return std::max(0.005, fitScale() / 8.0);
}

QPointF ImageView::imgToScreen(const QPointF& p) const
{
    return p * m_scale + m_pan;
}

QPointF ImageView::screenToImg(const QPointF& p) const
{
    return (p - m_pan) / m_scale;
}

#pragma once

#include <QImage>
#include <QPointF>
#include <QString>
#include <QWidget>

// Виджет просмотра одного изображения:
//   - зум колесом к позиции курсора, кнопками и горячими клавишами;
//   - панорамирование перетаскиванием (когда изображение больше окна);
//   - режимы «по размеру окна» и «1:1», двойной клик переключает их;
//   - шахматный фон под прозрачностью;
//   - состояние (масштаб + нормализованная точка в центре) для
//     синхронизации с другой панелью в режиме сравнения.
class ImageView : public QWidget {
    Q_OBJECT

public:
    // Нормализованные координаты (0..1) точки изображения в центре виджета.
    struct SyncState {
        double scale = 1.0;
        QPointF uv{0.5, 0.5};
    };

    explicit ImageView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void clear();
    bool hasImage() const { return !m_image.isNull(); }
    QImage image() const { return m_image; }

    double scale() const { return m_scale; }
    bool isFitMode() const { return m_fit; }

    void fitToWindow();
    void actualSize();
    void zoomIn();
    void zoomOut();

    void setBlankText(const QString& text);
    void setSyncEnabled(bool on) { m_syncEnabled = on; }

    SyncState syncState() const;
    void applySync(const SyncState& state);

signals:
    void zoomChanged(double scale);
    void fitModeChanged(bool fit);
    void syncChanged(const ImageView::SyncState& state);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateFit();
    void centerImage();
    void clampPan();
    void zoomAt(const QPointF& screenPos, double newScale);
    void notifyInteraction();
    bool imageOverflows() const;
    double fitScale() const;
    double minScale() const;
    QPointF imgToScreen(const QPointF& p) const;
    QPointF screenToImg(const QPointF& p) const;

    QImage m_image;
    double m_scale = 1.0;
    QPointF m_pan{0, 0};      // экранные координаты точки (0,0) изображения
    bool m_fit = true;

    bool m_dragging = false;
    QPoint m_pressPos;
    QPointF m_panAtPress;

    bool m_syncEnabled = false;
    bool m_applyingSync = false;
    QString m_blankText;
};

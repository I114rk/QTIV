#pragma once

#include <QList>
#include <QVector>
#include <QWidget>

#include "ImageView.h"

class QComboBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class ImageStore;
class QSplitter;

// Режим сравнения: N панелей ImageView (2 по умолчанию), выбор изображения
// для каждой панели и синхронные зум/панорамирование (опционально).
class CompareView : public QWidget {
    Q_OBJECT

public:
    static constexpr int kMaxPanelCount = 8;

    CompareView(ImageStore* store, int panels, QWidget* parent = nullptr);

    int panelCount() const { return m_panels.size(); }
    int panelIndex(int panel) const;
    void setPanelIndex(int panel, int index);
    // Задаёт количество панелей и фото на них (например, из CLI: -comp -n=3).
    void setIndices(const QVector<int>& indices);

    bool syncEnabled() const { return m_syncEnabled; }
    void setSyncEnabled(bool on);

    void retranslateUi();

signals:
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    struct Panel {
        ImageView* view = nullptr;
        QComboBox* combo = nullptr;
        int imageIndex = -1;
    };

    void setPanelCount(int count);
    int defaultImageIndex() const;
    void populateCombo(Panel& panel);
    void updatePanel(Panel& panel);
    void updateInfo();
    void updateSyncButtons();
    void applySyncFrom(ImageView* source);
    void applySyncToOthers(ImageView* source, const ImageView::SyncState& state);

    ImageStore* m_store;
    QList<Panel> m_panels;
    QHBoxLayout* m_barLayout = nullptr;
    QSplitter* m_splitter = nullptr;
    QPushButton* m_syncButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QLabel* m_infoLabel = nullptr;
    bool m_syncEnabled = true;
};

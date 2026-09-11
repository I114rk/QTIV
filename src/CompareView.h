#pragma once

#include <QWidget>

#include "ImageView.h"

class QComboBox;
class QLabel;
class QPushButton;
class ImageStore;

// Полноэкранный режим сравнения: две панели ImageView, выбор изображений
// для каждой панели и синхронные зум/панорамирование (опционально).
class CompareView : public QWidget {
    Q_OBJECT

public:
    CompareView(ImageStore* store, QWidget* parent = nullptr);

    int leftIndex() const { return m_leftIndex; }
    int rightIndex() const { return m_rightIndex; }
    void setLeftIndex(int index);
    void setRightIndex(int index);

    bool syncEnabled() const { return m_syncEnabled; }
    void setSyncEnabled(bool on);

    void retranslateUi();

signals:
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void populateCombo(QComboBox* combo, int currentIndex);
    void updatePanel(int side);
    void updateInfo();
    void updateSyncButtons();
    void applySyncFrom(ImageView* source);

    ImageStore* m_store;
    ImageView* m_left = nullptr;
    ImageView* m_right = nullptr;
    QComboBox* m_leftCombo = nullptr;
    QComboBox* m_rightCombo = nullptr;
    QPushButton* m_syncButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QLabel* m_infoLabel = nullptr;
    int m_leftIndex = -1;
    int m_rightIndex = -1;
    bool m_syncEnabled = true;
};

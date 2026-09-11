#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class ImageStore;

// Экспорт/конвертация: текущее фото или весь плейлист
// в PNG / JPEG / WebP / AVIF / BMP, либо упаковка нескольких фото
// в один альбом .qtivp.
class ExportDialog : public QDialog {
    Q_OBJECT

public:
    enum class Scope { CurrentImage, Playlist };

    ExportDialog(ImageStore* store, Scope scope, QWidget* parent = nullptr);

private slots:
    void browseDirectory();
    void formatChanged();
    void validate();

private:
    void run();
    void retranslateUi();

    QString defaultFileName(const QString& format) const;
    QString uniquePath(const QString& dir, const QString& name) const;
    bool exportImages(const QString& format, int quality, QString* summary);
    bool exportAlbum(const QString& format, QString* summary);

    ImageStore* m_store;
    Scope m_scope;

    QLineEdit* m_dirEdit = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_nameLabel = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QSlider* m_qualitySlider = nullptr;
    QLabel* m_qualityValue = nullptr;
    QLabel* m_qualityLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    QPushButton* m_okButton = nullptr;
    QString m_lastDir;
    bool m_nameEdited = false;
};

#pragma once

#include <QByteArray>
#include <QSettings>
#include <QString>

// Персональные настройки приложения (QSettings: ~/.config/QTIV/QTIV.conf).
class AppSettings {
public:
    static AppSettings& instance();

    int language() const;                              // 0 = системный, 1 = ru, 2 = en
    void setLanguage(int lang);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

    bool showThumbnails() const;
    void setShowThumbnails(bool on);

    bool syncCompare() const;
    void setSyncCompare(bool on);

    QString lastDir() const;
    void setLastDir(const QString& dir);

    int exportQuality() const;
    void setExportQuality(int quality);

    QString exportFormat() const;
    void setExportFormat(const QString& format);

private:
    AppSettings();
    QSettings m_settings;
};

#include "Settings.h"

#include <QDir>

AppSettings::AppSettings()
    : m_settings(QStringLiteral("QTIV"), QStringLiteral("QTIV"))
{
}

AppSettings& AppSettings::instance()
{
    static AppSettings s;
    return s;
}

int AppSettings::language() const
{
    return m_settings.value(QStringLiteral("ui/language"), 0).toInt();
}

void AppSettings::setLanguage(int lang)
{
    m_settings.setValue(QStringLiteral("ui/language"), lang);
}

QByteArray AppSettings::windowGeometry() const
{
    return m_settings.value(QStringLiteral("ui/geometry")).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray& geometry)
{
    m_settings.setValue(QStringLiteral("ui/geometry"), geometry);
}

bool AppSettings::showThumbnails() const
{
    return m_settings.value(QStringLiteral("ui/showThumbnails"), true).toBool();
}

void AppSettings::setShowThumbnails(bool on)
{
    m_settings.setValue(QStringLiteral("ui/showThumbnails"), on);
}

bool AppSettings::syncCompare() const
{
    return m_settings.value(QStringLiteral("view/syncCompare"), true).toBool();
}

void AppSettings::setSyncCompare(bool on)
{
    m_settings.setValue(QStringLiteral("view/syncCompare"), on);
}

QString AppSettings::lastDir() const
{
    return m_settings.value(QStringLiteral("paths/lastDir"), QDir::homePath()).toString();
}

void AppSettings::setLastDir(const QString& dir)
{
    m_settings.setValue(QStringLiteral("paths/lastDir"), dir);
}

int AppSettings::exportQuality() const
{
    return m_settings.value(QStringLiteral("export/quality"), 90).toInt();
}

void AppSettings::setExportQuality(int quality)
{
    m_settings.setValue(QStringLiteral("export/quality"), quality);
}

QString AppSettings::exportFormat() const
{
    return m_settings.value(QStringLiteral("export/format"), QStringLiteral("png")).toString();
}

void AppSettings::setExportFormat(const QString& format)
{
    m_settings.setValue(QStringLiteral("export/format"), format);
}

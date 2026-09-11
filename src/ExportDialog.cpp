#include "ExportDialog.h"
#include "ImageStore.h"
#include "Locale.h"
#include "Qtivp.h"
#include "Settings.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QSlider>
#include <QVBoxLayout>

namespace {

struct FormatInfo {
    const char* key;      // ключ Qt: "png", "jpeg", ...
    QString suffix;       // расширение файла
};

const QList<FormatInfo>& formats()
{
    static const QList<FormatInfo> list = {
        {"png",   QStringLiteral("png")},
        {"jpeg",  QStringLiteral("jpg")},
        {"webp",  QStringLiteral("webp")},
        {"avif",  QStringLiteral("avif")},
        {"bmp",   QStringLiteral("bmp")},
        {"qtivp", QStringLiteral("qtivp")},
    };
    return list;
}

} // namespace

ExportDialog::ExportDialog(ImageStore* store, Scope scope, QWidget* parent)
    : QDialog(parent)
    , m_store(store)
    , m_scope(scope)
{
    setMinimumWidth(480);
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    m_lastDir = AppSettings::instance().lastDir();

    // --- Папка назначения -----------------------------------------------
    auto* dirRow = new QHBoxLayout;
    m_dirEdit = new QLineEdit(m_lastDir, this);
    auto* browseButton = new QPushButton(i18n::s("Browse…"), this);
    dirRow->addWidget(m_dirEdit, 1);
    dirRow->addWidget(browseButton);
    root->addWidget(new QLabel(i18n::s("Destination folder:"), this));
    root->addLayout(dirRow);

    // --- Имя файла (для одного файла / альбома) --------------------------
    m_nameLabel = new QLabel(i18n::s("File name:"), this);
    m_nameEdit = new QLineEdit(this);
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(m_nameEdit, 1);
    root->addWidget(m_nameLabel);
    root->addLayout(nameRow);

    // --- Формат -----------------------------------------------------------
    root->addWidget(new QLabel(i18n::s("Format:"), this));
    m_formatCombo = new QComboBox(this);
    for (const FormatInfo& f : formats()) {
        QString label;
        if (qstrcmp(f.key, "qtivp") == 0)
            label = i18n::s("QTIVP album (photos in one file)");
        else
            label = ImageStore::prettyFormat(QString::fromLatin1(f.key));
        m_formatCombo->addItem(label, QString::fromLatin1(f.key));
    }
    root->addWidget(m_formatCombo);

    // --- Качество ----------------------------------------------------------
    m_qualityLabel = new QLabel(i18n::s("Quality:"), this);
    auto* qualityRow = new QHBoxLayout;
    m_qualitySlider = new QSlider(Qt::Horizontal, this);
    m_qualitySlider->setRange(1, 100);
    m_qualitySlider->setValue(AppSettings::instance().exportQuality());
    m_qualityValue = new QLabel(this);
    m_qualityValue->setMinimumWidth(36);
    qualityRow->addWidget(m_qualitySlider, 1);
    qualityRow->addWidget(m_qualityValue);
    root->addWidget(m_qualityLabel);
    root->addLayout(qualityRow);

    // --- Пояснение ----------------------------------------------------------
    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setStyleSheet(QStringLiteral("color: #9aa1b2;"));
    root->addWidget(m_infoLabel);

    root->addStretch(1);

    // --- Кнопки ---------------------------------------------------------------
    auto* buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    auto* cancelButton = new QPushButton(i18n::s("Cancel"), this);
    m_okButton = new QPushButton(i18n::s("Export"), this);
    m_okButton->setDefault(true);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(m_okButton);
    root->addLayout(buttonRow);

    connect(browseButton, &QPushButton::clicked, this, &ExportDialog::browseDirectory);
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::formatChanged);
    connect(m_nameEdit, &QLineEdit::textEdited, this, [this] { m_nameEdited = true; });
    connect(m_qualitySlider, &QSlider::valueChanged, this, [this](int value) {
        m_qualityValue->setText(QString::number(value));
        validate();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this, &ExportDialog::run);

    // Восстановить прошлый формат.
    const QString savedFormat = AppSettings::instance().exportFormat();
    const int pos = m_formatCombo->findData(savedFormat);
    m_formatCombo->setCurrentIndex(pos >= 0 ? pos : 0);

    m_qualityValue->setText(QString::number(m_qualitySlider->value()));
    formatChanged();
    retranslateUi();
}

void ExportDialog::retranslateUi()
{
    if (m_scope == Scope::CurrentImage)
        setWindowTitle(i18n::s("Convert image"));
    else
        setWindowTitle(i18n::s("Export playlist"));
    formatChanged();
}

void ExportDialog::formatChanged()
{
    const QString format = m_formatCombo->currentData().toString();
    const bool isAlbum = format == QLatin1String("qtivp");
    const bool needsFileName = (m_scope == Scope::CurrentImage) || isAlbum;
    m_nameLabel->setVisible(needsFileName);
    m_nameEdit->setVisible(needsFileName);
    if (needsFileName && !m_nameEdited)
        m_nameEdit->setText(defaultFileName(format));

    const bool hasQuality = !isAlbum && format != QLatin1String("png") && format != QLatin1String("bmp");
    m_qualityLabel->setVisible(hasQuality);
    m_qualitySlider->setVisible(hasQuality);
    m_qualityValue->setVisible(hasQuality);

    if (isAlbum) {
        m_infoLabel->setText(m_scope == Scope::Playlist
                                 ? i18n::s("All photos from the playlist will be packed into one .qtivp album. Photos are stored without re-encoding.")
                                 : i18n::s("The photo will be packed into a .qtivp album without re-encoding."));
    } else if (m_scope == Scope::Playlist) {
        m_infoLabel->setText(i18n::s("Each photo from the playlist will be converted and saved to the destination folder."));
    } else {
        m_infoLabel->setText(QString());
    }
    validate();
}

QString ExportDialog::defaultFileName(const QString& format) const
{
    const ImageStore::Item* it = m_store->currentItem();
    const QString base = it ? QFileInfo(it->displayName).completeBaseName()
                            : QStringLiteral("image");
    const QString suffix = format == QLatin1String("qtivp")
                               ? QStringLiteral("qtivp")
                               : format == QLatin1String("jpeg") ? QStringLiteral("jpg") : format;
    return base + QLatin1Char('.') + suffix;
}

void ExportDialog::browseDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, i18n::s("Select destination folder"), m_dirEdit->text());
    if (!dir.isEmpty())
        m_dirEdit->setText(dir);
}

void ExportDialog::validate()
{
    const QString format = m_formatCombo->currentData().toString();
    const bool isAlbum = format == QLatin1String("qtivp");
    const bool needsName = (m_scope == Scope::CurrentImage) || isAlbum;
    const bool ok = QDir().exists(m_dirEdit->text())
        && (!needsName || !m_nameEdit->text().trimmed().isEmpty());
    m_okButton->setEnabled(ok);
}

QString ExportDialog::uniquePath(const QString& dir, const QString& name) const
{
    QFileInfo fi(QDir(dir), name);
    if (!fi.exists())
        return fi.absoluteFilePath();
    const QString base = fi.completeBaseName();
    const QString suffix = fi.suffix();
    for (int i = 1; i < 10000; ++i) {
        const QString candidate = suffix.isEmpty()
            ? QStringLiteral("%1-%2").arg(base).arg(i)
            : QStringLiteral("%1-%2.%3").arg(base).arg(i).arg(suffix);
        QFileInfo cf(QDir(dir), candidate);
        if (!cf.exists())
            return cf.absoluteFilePath();
    }
    return fi.absoluteFilePath();
}

void ExportDialog::run()
{
    const QString format = m_formatCombo->currentData().toString();
    const int quality = m_qualitySlider->value();
    const QString dir = m_dirEdit->text();

    AppSettings::instance().setLastDir(dir);
    AppSettings::instance().setExportQuality(quality);
    AppSettings::instance().setExportFormat(format);

    // Подтверждение перезаписи.
    QStringList existing;
    if (format == QLatin1String("qtivp")) {
        const QString target = QFileInfo(QDir(dir), m_nameEdit->text().trimmed()).absoluteFilePath();
        if (QFile::exists(target))
            existing << target;
    } else if (m_scope == Scope::CurrentImage) {
        const QString target = QFileInfo(QDir(dir), m_nameEdit->text().trimmed()).absoluteFilePath();
        if (QFile::exists(target))
            existing << target;
    } else {
        const QString suffix = format == QLatin1String("jpeg") ? QStringLiteral("jpg") : format;
        for (int i = 0; i < m_store->count(); ++i) {
            const QString base = QFileInfo(m_store->item(i).displayName).completeBaseName();
            const QFileInfo fi(QDir(dir), base + QLatin1Char('.') + suffix);
            if (fi.exists())
                existing << fi.absoluteFilePath();
        }
    }
    if (!existing.isEmpty()) {
        const QString question = existing.size() == 1
            ? i18n::s("File already exists. Overwrite?")
            : i18n::s("%1 files already exist. Overwrite?").arg(existing.size());
        if (QMessageBox::question(this, windowTitle(), question) != QMessageBox::Yes)
            return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString summary;
    bool ok = (format == QLatin1String("qtivp"))
        ? exportAlbum(format, &summary)
        : exportImages(format, quality, &summary);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, windowTitle(), summary);
        return;
    }
    QMessageBox::information(this, windowTitle(), summary);
    accept();
}

bool ExportDialog::exportImages(const QString& format, int quality, QString* summary)
{
    const QString suffix = format == QLatin1String("jpeg") ? QStringLiteral("jpg") : format;
    const QString dir = m_dirEdit->text();
    QStringList errors;

    if (m_scope == Scope::CurrentImage) {
        const int index = m_store->current();
        if (index < 0) {
            *summary = i18n::s("No image selected.");
            return false;
        }
        const QImage img = m_store->image(index);
        if (img.isNull()) {
            *summary = i18n::s("Could not decode the image.");
            return false;
        }
        QString name = m_nameEdit->text().trimmed();
        if (QFileInfo(name).suffix().isEmpty())
            name += QLatin1Char('.') + suffix;
        const QString target = QFileInfo(QDir(dir), name).absoluteFilePath();

        QImage out = img;
        if (format == QLatin1String("jpeg") && out.hasAlphaChannel()) {
            QImage flat(out.size(), QImage::Format_RGB32);
            flat.fill(Qt::white);
            QPainter painter(&flat);
            painter.drawImage(0, 0, out);
            out = flat;
        }

        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly)) {
            *summary = QStringLiteral("%1: %2").arg(target, file.errorString());
            return false;
        }
        QImageWriter writer(&file, format.toLatin1());
        writer.setQuality(quality);
        if (!writer.write(out) || !file.commit()) {
            *summary = QStringLiteral("%1: %2").arg(target, writer.errorString());
            return false;
        }
        *summary = i18n::s("Saved: %1").arg(target);
        return true;
    }

    int saved = 0;
    for (int i = 0; i < m_store->count(); ++i) {
        const QImage img = m_store->image(i);
        if (img.isNull()) {
            errors << m_store->item(i).displayName;
            continue;
        }
        const QString base = QFileInfo(m_store->item(i).displayName).completeBaseName();
        const QString target = uniquePath(dir, base + QLatin1Char('.') + suffix);

        QImage out = img;
        if (format == QLatin1String("jpeg") && out.hasAlphaChannel()) {
            QImage flat(out.size(), QImage::Format_RGB32);
            flat.fill(Qt::white);
            QPainter painter(&flat);
            painter.drawImage(0, 0, out);
            out = flat;
        }

        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly)) {
            errors << target + QStringLiteral(": ") + file.errorString();
            continue;
        }
        QImageWriter writer(&file, format.toLatin1());
        writer.setQuality(quality);
        if (!writer.write(out) || !file.commit()) {
            errors << target + QStringLiteral(": ") + writer.errorString();
            continue;
        }
        ++saved;
    }

    if (errors.isEmpty()) {
        *summary = i18n::s("Exported %1 files to %2").arg(saved).arg(m_dirEdit->text());
        return true;
    }
    *summary = i18n::s("Exported %1 files, %2 failed:\n%3")
                   .arg(saved)
                   .arg(errors.size())
                   .arg(errors.first(12).join(u'\n'));
    return errors.isEmpty();
}

bool ExportDialog::exportAlbum(const QString& format, QString* summary)
{
    Q_UNUSED(format);
    const QString dir = m_dirEdit->text();
    QString name = m_nameEdit->text().trimmed();
    if (QFileInfo(name).suffix().compare(QStringLiteral("qtivp"), Qt::CaseInsensitive) != 0)
        name += QStringLiteral(".qtivp");
    const QString target = QFileInfo(QDir(dir), name).absoluteFilePath();

    Qtivp::EntryList entries;
    if (m_scope == Scope::CurrentImage) {
        const int index = m_store->current();
        if (index < 0) {
            *summary = i18n::s("No image selected.");
            return false;
        }
        const ImageStore::Item& it = m_store->item(index);
        Qtivp::Entry e;
        e.name = it.displayName;
        e.mime = it.mime.isEmpty() ? Qtivp::mimeForFormat(it.format) : it.mime;
        e.blob = m_store->encoded(index);
        e.width = quint32(it.width);
        e.height = quint32(it.height);
        if (e.blob.isEmpty()) {
            *summary = i18n::s("Could not read the image data.");
            return false;
        }
        entries.append(e);
    } else {
        for (int i = 0; i < m_store->count(); ++i) {
            const ImageStore::Item& it = m_store->item(i);
            Qtivp::Entry e;
            e.name = it.displayName;
            e.mime = it.mime.isEmpty() ? Qtivp::mimeForFormat(it.format) : it.mime;
            e.blob = m_store->encoded(i);
            e.width = quint32(it.width);
            e.height = quint32(it.height);
            if (e.blob.isEmpty())
                continue; // пропускаем нечитаемые
            entries.append(e);
        }
    }
    if (entries.isEmpty()) {
        *summary = i18n::s("No readable photos to pack.");
        return false;
    }

    QString error;
    if (!Qtivp::Writer::write(target, entries, &error)) {
        *summary = error;
        return false;
    }
    *summary = i18n::s("Packed %1 photos into %2").arg(entries.size()).arg(target);
    return true;
}

#include "MainWindow.h"
#include "CompareView.h"
#include "ExportDialog.h"
#include "ImageStore.h"
#include "ImageView.h"
#include "Locale.h"
#include "Qtivp.h"
#include "Settings.h"
#include "ThumbnailStrip.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QSet>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("QTIV"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/qtiv.svg")));
    setAcceptDrops(true);
    resize(1180, 780);

    m_store = new ImageStore(this);

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    auto* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    m_view = new ImageView(page);
    pageLayout->addWidget(m_view, 1);
    m_strip = new ThumbnailStrip(m_store, page);
    pageLayout->addWidget(m_strip);
    m_stack->addWidget(page);

    createActions();
    createMenus();
    createToolBar();
    createStatusBar();

    connect(m_store, &ImageStore::currentChanged, this, &MainWindow::onCurrentChanged);
    connect(m_view, &ImageView::zoomChanged, this, [this](double scale) {
        m_statusZoom->setText(QStringLiteral("%1%").arg(qRound(scale * 100.0)));
    });

    const QByteArray geometry = AppSettings::instance().windowGeometry();
    if (geometry.isEmpty() || !restoreGeometry(geometry)) {
        const QRect avail = screen()->availableGeometry();
        resize(avail.width() * 2 / 3, avail.height() * 2 / 3);
        move(avail.center() - QPoint(width() / 2, height() / 2));
    }
    m_thumbsAction->setChecked(AppSettings::instance().showThumbnails());

    retranslateUi();
    updateNavState();
    updateInfoBar();
}

bool MainWindow::loadPaths(const QStringList& paths)
{
    const QString error = m_store->openPaths(paths);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, i18n::s("Open error"), error);
        return false;
    }
    return true;
}

void MainWindow::openDialog()
{
    QString filters = ImageStore::nameFilters().join(u' ');
    filters += QStringLiteral(";;") + i18n::s("All files (*)");
    const QStringList files = QFileDialog::getOpenFileNames(
        this, i18n::s("Open images"), AppSettings::instance().lastDir(), filters);
    if (files.isEmpty())
        return;
    AppSettings::instance().setLastDir(QFileInfo(files.first()).absolutePath());
    loadPaths(files);
}

void MainWindow::openAlbumDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, i18n::s("Open .qtivp album"), AppSettings::instance().lastDir(),
        i18n::s("QTIVP albums (*.qtivp)"));
    if (!path.isEmpty())
        loadPaths({path});
}

void MainWindow::extractAlbum()
{
    const ImageStore::Item* it = m_store->currentItem();
    if (!it || !it->fromAlbum())
        return;
    const QString albumPath = it->sourcePath;

    const QString dir = QFileDialog::getExistingDirectory(
        this, i18n::s("Extract album to folder"), AppSettings::instance().lastDir());
    if (dir.isEmpty())
        return;
    AppSettings::instance().setLastDir(dir);

    Qtivp::EntryList entries;
    QString error;
    if (!Qtivp::Reader::openIndex(albumPath, entries, &error)) {
        QMessageBox::warning(this, i18n::s("Album error"), error);
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList saved;
    QStringList errors;
    QSet<QString> usedNames;
    for (const Qtivp::Entry& e : entries) {
        QString name = QFileInfo(e.name).fileName(); // защита от путей в имени записи
        if (name.isEmpty() || name == QLatin1String("."))
            name = QStringLiteral("image");
        QString base = QFileInfo(name).completeBaseName();
        QString suffix = QFileInfo(name).suffix();
        QString candidate = name;
        int counter = 1;
        while (usedNames.contains(candidate)
               || QFileInfo::exists(QDir(dir).absoluteFilePath(candidate))) {
            candidate = suffix.isEmpty()
                ? QStringLiteral("%1-%2").arg(base).arg(counter)
                : QStringLiteral("%1-%2.%3").arg(base).arg(counter).arg(suffix);
            ++counter;
        }
        usedNames.insert(candidate);

        QString blobError;
        const QByteArray blob = Qtivp::Reader::readBlob(albumPath, e, &blobError);
        if (blob.isEmpty()) {
            errors << QStringLiteral("%1: %2").arg(candidate, blobError);
            continue;
        }
        QFile out(QDir(dir).absoluteFilePath(candidate));
        if (!out.open(QIODevice::WriteOnly)) {
            errors << QStringLiteral("%1: %2").arg(candidate, out.errorString());
            continue;
        }
        out.write(blob);
        saved << candidate;
    }
    QApplication::restoreOverrideCursor();

    QString message = i18n::s("Extracted %1 photos to %2").arg(saved.size()).arg(dir);
    if (!errors.isEmpty())
        message += QStringLiteral("\n\n") + i18n::s("Failed:")
            + QStringLiteral("\n") + errors.first(12).join(u'\n');
    QMessageBox::information(this, i18n::s("Extract album"), message);
}

void MainWindow::exportCurrent()
{
    if (m_store->count() == 0)
        return;
    ExportDialog dialog(m_store, ExportDialog::Scope::CurrentImage, this);
    dialog.exec();
}

void MainWindow::exportPlaylist()
{
    if (m_store->count() == 0)
        return;
    ExportDialog dialog(m_store, ExportDialog::Scope::Playlist, this);
    dialog.exec();
}

void MainWindow::onCurrentChanged(int index)
{
    m_posSpin->blockSignals(true);
    m_posSpin->setValue(index + 1);
    m_posSpin->blockSignals(false);
    updateView();
}

void MainWindow::toggleCompare(bool on)
{
    if (on) {
        if (m_store->count() < 2) {
            m_compareAction->setChecked(false);
            return;
        }
        if (!m_compare) {
            m_compare = new CompareView(m_store, this);
            m_compare->setSyncEnabled(AppSettings::instance().syncCompare());
            m_stack->addWidget(m_compare);
            connect(m_compare, &CompareView::closed, this, [this] {
                m_compareAction->setChecked(false);
            });
        }
        m_stack->setCurrentWidget(m_compare);
    } else {
        m_stack->setCurrentIndex(0);
    }
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void MainWindow::showSpec()
{
    QMessageBox::information(
        this, i18n::s("About the QTIVP format"),
        i18n::s("QTIVP (.qtivp) is an open container for storing several photos in "
                "one file: an 8-byte magic header, an entry index (name, MIME type, "
                "dimensions, offsets, CRC32) and image data, optionally compressed "
                "with zlib. Photos are stored without re-encoding.\n\n"
                "Full specification: docs/QTIVP-SPEC.md in the QTIV repository."));
}

void MainWindow::createActions()
{
    auto add = [this](const char* icon, QKeySequence::StandardKey standard) {
        QAction* a = new QAction(this);
        if (icon)
            a->setIcon(QIcon(QLatin1String(":/assets/icons/") + QLatin1String(icon)));
        if (standard != QKeySequence::UnknownKey)
            a->setShortcut(QKeySequence(standard));
        return a;
    };

    m_openAction = add("open.svg", QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openDialog);

    m_openAlbumAction = new QAction(QIcon(QStringLiteral(":/assets/icons/album.svg")), QString(), this);
    m_openAlbumAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(m_openAlbumAction, &QAction::triggered, this, &MainWindow::openAlbumDialog);

    m_extractAction = new QAction(QIcon(QStringLiteral(":/assets/icons/album.svg")), QString(), this);
    m_extractAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    connect(m_extractAction, &QAction::triggered, this, &MainWindow::extractAlbum);

    m_exportCurrentAction = new QAction(QIcon(QStringLiteral(":/assets/icons/export.svg")), QString(), this);
    m_exportCurrentAction->setShortcut(QKeySequence::Save);
    connect(m_exportCurrentAction, &QAction::triggered, this, &MainWindow::exportCurrent);

    m_exportPlaylistAction = new QAction(QIcon(QStringLiteral(":/assets/icons/export.svg")), QString(), this);
    m_exportPlaylistAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(m_exportPlaylistAction, &QAction::triggered, this, &MainWindow::exportPlaylist);

    m_quitAction = add(nullptr, QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_prevAction = new QAction(QIcon(QStringLiteral(":/assets/icons/prev.svg")), QString(), this);
    m_prevAction->setShortcuts({QKeySequence(Qt::Key_Left), QKeySequence(Qt::Key_PageUp)});
    connect(m_prevAction, &QAction::triggered, this, [this] { m_store->previous(); });

    m_nextAction = new QAction(QIcon(QStringLiteral(":/assets/icons/next.svg")), QString(), this);
    m_nextAction->setShortcuts({QKeySequence(Qt::Key_Right), QKeySequence(Qt::Key_PageDown)});
    connect(m_nextAction, &QAction::triggered, this, [this] { m_store->next(); });

    m_firstAction = new QAction(this);
    m_firstAction->setShortcut(QKeySequence(Qt::Key_Home));
    connect(m_firstAction, &QAction::triggered, this, [this] { m_store->setCurrent(0); });

    m_lastAction = new QAction(this);
    m_lastAction->setShortcut(QKeySequence(Qt::Key_End));
    connect(m_lastAction, &QAction::triggered, this, [this] {
        m_store->setCurrent(m_store->count() - 1);
    });

    m_zoomInAction = new QAction(QIcon(QStringLiteral(":/assets/icons/zoom-in.svg")), QString(), this);
    m_zoomInAction->setShortcuts({QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    connect(m_zoomInAction, &QAction::triggered, m_view, &ImageView::zoomIn);

    m_zoomOutAction = new QAction(QIcon(QStringLiteral(":/assets/icons/zoom-out.svg")), QString(), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, m_view, &ImageView::zoomOut);

    m_fitAction = new QAction(QIcon(QStringLiteral(":/assets/icons/fit.svg")), QString(), this);
    m_fitAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(m_fitAction, &QAction::triggered, m_view, &ImageView::fitToWindow);

    m_actualAction = new QAction(QIcon(QStringLiteral(":/assets/icons/actual.svg")), QString(), this);
    m_actualAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(m_actualAction, &QAction::triggered, m_view, &ImageView::actualSize);

    m_compareAction = new QAction(QIcon(QStringLiteral(":/assets/icons/compare.svg")), QString(), this);
    m_compareAction->setCheckable(true);
    m_compareAction->setShortcuts({QKeySequence(Qt::Key_F12), QKeySequence(Qt::Key_C)});
    connect(m_compareAction, &QAction::toggled, this, &MainWindow::toggleCompare);

    m_thumbsAction = new QAction(this);
    m_thumbsAction->setCheckable(true);
    m_thumbsAction->setChecked(true);
    m_thumbsAction->setShortcut(QKeySequence(Qt::Key_T));
    connect(m_thumbsAction, &QAction::toggled, this, [this](bool on) {
        AppSettings::instance().setShowThumbnails(on);
        m_strip->setVisible(on && m_store->count() > 0);
    });

    m_fullscreenAction = new QAction(QIcon(QStringLiteral(":/assets/icons/fullscreen.svg")), QString(), this);
    m_fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(m_fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);

    m_specAction = new QAction(this);
    connect(m_specAction, &QAction::triggered, this, &MainWindow::showSpec);

    m_aboutAction = new QAction(this);
    connect(m_aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(this, i18n::s("About QTIV"),
                           QStringLiteral("<b>QTIV %1</b><br>").arg(QTIV_VERSION)
                               + i18n::s("Image viewer, comparison and conversion. "
                                         "Supports the open .qtivp album format.")
                               + QStringLiteral("<br><br>")
                               + i18n::s("License: MIT"));
    });

    m_aboutQtAction = new QAction(this);
    connect(m_aboutQtAction, &QAction::triggered, this, [this] {
        QMessageBox::aboutQt(this, i18n::s("About Qt"));
    });

    // --- Язык интерфейса ---------------------------------------------------
    m_langGroup = new QActionGroup(this);
    for (int i = 0; i < 3; ++i) {
        m_langActions[i] = new QAction(this);
        m_langActions[i]->setCheckable(true);
        m_langActions[i]->setData(i);
        m_langGroup->addAction(m_langActions[i]);
        connect(m_langActions[i], &QAction::triggered, this, &MainWindow::changeLanguage);
    }
    const int savedLang = AppSettings::instance().language();
    if (savedLang >= 0 && savedLang < 3)
        m_langActions[savedLang]->setChecked(true);
    else
        m_langActions[0]->setChecked(true);
}

void MainWindow::createMenus()
{
    m_fileMenu = menuBar()->addMenu(QString());
    m_fileMenu->addAction(m_openAction);
    m_fileMenu->addAction(m_openAlbumAction);
    m_fileMenu->addAction(m_extractAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exportCurrentAction);
    m_fileMenu->addAction(m_exportPlaylistAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_quitAction);

    m_viewMenu = menuBar()->addMenu(QString());
    m_viewMenu->addAction(m_prevAction);
    m_viewMenu->addAction(m_nextAction);
    m_viewMenu->addAction(m_firstAction);
    m_viewMenu->addAction(m_lastAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_zoomInAction);
    m_viewMenu->addAction(m_zoomOutAction);
    m_viewMenu->addAction(m_fitAction);
    m_viewMenu->addAction(m_actualAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_thumbsAction);
    m_viewMenu->addAction(m_compareAction);
    m_viewMenu->addAction(m_fullscreenAction);

    m_settingsMenu = menuBar()->addMenu(QString());
    m_langMenu = m_settingsMenu->addMenu(QString());
    for (QAction* a : m_langActions)
        m_langMenu->addAction(a);

    m_helpMenu = menuBar()->addMenu(QString());
    m_helpMenu->addAction(m_specAction);
    m_helpMenu->addSeparator();
    m_helpMenu->addAction(m_aboutAction);
    m_helpMenu->addAction(m_aboutQtAction);
}

void MainWindow::createToolBar()
{
    m_toolbar = addToolBar(QString());
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(20, 20));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_toolbar->addAction(m_openAction);
    m_toolbar->addSeparator();

    m_toolbar->addAction(m_prevAction);
    m_posSpin = new QSpinBox(this);
    m_posSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_posSpin->setKeyboardTracking(false);
    m_posSpin->setFixedWidth(64);
    m_posSpin->setAlignment(Qt::AlignCenter);
    m_posSpin->setRange(1, 1);
    connect(m_posSpin, &QSpinBox::valueChanged, this, [this](int value) {
        m_store->setCurrent(value - 1);
    });
    m_toolbar->addWidget(m_posSpin);
    m_countLabel = new QLabel(this);
    m_toolbar->addWidget(m_countLabel);
    m_toolbar->addAction(m_nextAction);
    m_toolbar->addSeparator();

    m_toolbar->addAction(m_zoomOutAction);
    m_toolbar->addAction(m_zoomInAction);
    m_toolbar->addAction(m_fitAction);
    m_toolbar->addAction(m_actualAction);
    m_toolbar->addSeparator();

    m_toolbar->addAction(m_compareAction);
    m_toolbar->addAction(m_exportCurrentAction);
    m_toolbar->addSeparator();

    m_toolbar->addAction(m_fullscreenAction);
}

void MainWindow::createStatusBar()
{
    m_statusName = new QLabel(this);
    m_statusName->setMinimumWidth(240);
    m_statusMeta = new QLabel(this);
    m_statusMeta->setMinimumWidth(160);
    m_statusZoom = new QLabel(this);
    statusBar()->addWidget(m_statusName, 2);
    statusBar()->addWidget(m_statusMeta, 1);
    statusBar()->addPermanentWidget(m_statusZoom);
}

void MainWindow::updateView()
{
    const int index = m_store->current();
    if (index < 0) {
        m_view->clear();
        m_view->setBlankText(i18n::s("Open an image or a folder (Ctrl+O)\n"
                                     "or drag and drop files here"));
    } else {
        const QImage img = m_store->image(index);
        if (img.isNull()) {
            const QString err = m_store->lastDecodeError();
            m_view->clear();
            m_view->setBlankText(i18n::s("Could not decode image")
                + (err.isEmpty() ? QString() : QStringLiteral("\n") + err));
        } else {
            m_view->setImage(img);
        }
    }
    updateNavState();
    updateInfoBar();
}

void MainWindow::updateNavState()
{
    const int n = m_store->count();
    m_prevAction->setEnabled(n > 1);
    m_nextAction->setEnabled(n > 1);
    m_firstAction->setEnabled(n > 1);
    m_lastAction->setEnabled(n > 1);
    m_posSpin->setEnabled(n > 0);
    m_posSpin->setRange(1, qMax(n, 1));
    m_countLabel->setText(n > 0 ? i18n::s("of %1").arg(n) : QString());
    m_strip->setVisible(n > 0 && m_thumbsAction->isChecked());
    m_exportCurrentAction->setEnabled(n > 0);
    m_exportPlaylistAction->setEnabled(n > 0);

    const ImageStore::Item* cur = m_store->currentItem();
    m_extractAction->setEnabled(cur && cur->fromAlbum());

    m_compareAction->setEnabled(n >= 2);
    if (!m_compareAction->isEnabled() && m_compareAction->isChecked())
        m_compareAction->setChecked(false);
}

void MainWindow::updateInfoBar()
{
    const ImageStore::Item* it = m_store->currentItem();
    if (!it) {
        m_statusName->setText(i18n::s("No image"));
        m_statusMeta->clear();
        m_statusZoom->clear();
        return;
    }
    if (it->fromAlbum()) {
        m_statusName->setText(QStringLiteral("%1 ▸ %2")
                                  .arg(QFileInfo(it->sourcePath).fileName(), it->displayName));
    } else {
        m_statusName->setText(it->sourcePath);
    }

    QStringList meta;
    if (it->width > 0)
        meta << QStringLiteral("%1×%2").arg(it->width).arg(it->height);
    meta << i18n::humanSize(it->byteSize);
    if (!it->format.isEmpty())
        meta << ImageStore::prettyFormat(it->format);
    m_statusMeta->setText(meta.join(QStringLiteral("  ·  ")));
}

void MainWindow::changeLanguage()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action)
        return;
    const int lang = action->data().toInt();
    AppSettings::instance().setLanguage(lang);
    i18n::setLang(static_cast<i18n::Lang>(lang));
    retranslateUi();
}

void MainWindow::retranslateUi()
{
    m_openAction->setText(i18n::s("&Open…"));
    m_openAction->setStatusTip(i18n::s("Open images, a folder or a .qtivp album"));
    m_openAlbumAction->setText(i18n::s("Open &album…"));
    m_openAlbumAction->setStatusTip(i18n::s("Open a .qtivp album"));
    m_extractAction->setText(i18n::s("&Extract album…"));
    m_extractAction->setStatusTip(i18n::s("Save all photos from the current album to a folder"));
    m_exportCurrentAction->setText(i18n::s("&Convert image…"));
    m_exportCurrentAction->setStatusTip(i18n::s("Convert the current image to another format"));
    m_exportPlaylistAction->setText(i18n::s("Export &playlist…"));
    m_exportPlaylistAction->setStatusTip(i18n::s("Convert all photos or pack them into a .qtivp album"));
    m_quitAction->setText(i18n::s("&Quit"));

    m_prevAction->setText(i18n::s("&Previous"));
    m_nextAction->setText(i18n::s("&Next"));
    m_firstAction->setText(i18n::s("&First image"));
    m_lastAction->setText(i18n::s("&Last image"));
    m_zoomInAction->setText(i18n::s("Zoom &in"));
    m_zoomOutAction->setText(i18n::s("Zoom &out"));
    m_fitAction->setText(i18n::s("Fit to &window"));
    m_actualAction->setText(i18n::s("Actual si&ze"));
    m_compareAction->setText(i18n::s("&Compare"));
    m_compareAction->setStatusTip(i18n::s("Show two images side by side (sync zoom)"));
    m_thumbsAction->setText(i18n::s("&Thumbnails"));
    m_thumbsAction->setStatusTip(i18n::s("Show or hide the thumbnail strip"));
    m_fullscreenAction->setText(i18n::s("&Fullscreen"));

    m_specAction->setText(i18n::s("QTIVP &format…"));
    m_aboutAction->setText(i18n::s("&About QTIV"));
    m_aboutQtAction->setText(i18n::s("About &Qt"));

    m_fileMenu->setTitle(i18n::s("&File"));
    m_viewMenu->setTitle(i18n::s("&View"));
    m_settingsMenu->setTitle(i18n::s("&Settings"));
    m_langMenu->setTitle(i18n::s("&Language"));
    m_helpMenu->setTitle(i18n::s("&Help"));
    m_langActions[0]->setText(i18n::s("System"));
    m_langActions[1]->setText(QStringLiteral("Русский"));
    m_langActions[2]->setText(QStringLiteral("English"));

    m_posSpin->setToolTip(i18n::s("Go to image number"));
    m_countLabel->setToolTip(i18n::s("Images in the playlist"));

    m_view->setBlankText(m_store->count() == 0
                             ? i18n::s("Open an image or a folder (Ctrl+O)\n"
                                       "or drag and drop files here")
                             : i18n::s("Could not decode image"));
    if (m_compare)
        m_compare->retranslateUi();

    updateNavState();
    updateInfoBar();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    QStringList paths;
    const auto urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (!paths.isEmpty())
        loadPaths(paths);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    AppSettings::instance().setWindowGeometry(saveGeometry());
    QMainWindow::closeEvent(event);
}

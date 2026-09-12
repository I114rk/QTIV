#pragma once

#include <QMainWindow>
#include <QVector>

class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QSpinBox;
class QStackedWidget;
class QToolBar;
class CompareView;
class HelpWindow;
class ImageStore;
class ImageView;
class ThumbnailStrip;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    bool loadPaths(const QStringList& paths);

    int playlistCount() const;
    int currentIndex() const;
    // Открывает режим сравнения; пустой список — пара «текущее + следующее».
    void showCompare(const QVector<int>& indices = {});

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openDialog();
    void openAlbumDialog();
    void extractAlbum();
    void exportCurrent();
    void exportPlaylist();
    void onCurrentChanged(int index);
    void toggleCompare(bool on);
    void toggleFullscreen();
    void showSpec();
    void showHelp();
    void changeLanguage();

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void updateView();
    void updateNavState();
    void updateInfoBar();
    void retranslateUi();
    void ensureCompareView(int panels);

    ImageStore* m_store = nullptr;
    QStackedWidget* m_stack = nullptr;
    ImageView* m_view = nullptr;
    ThumbnailStrip* m_strip = nullptr;
    CompareView* m_compare = nullptr;
    HelpWindow* m_help = nullptr;

    QAction* m_openAction = nullptr;
    QAction* m_openAlbumAction = nullptr;
    QAction* m_extractAction = nullptr;
    QAction* m_exportCurrentAction = nullptr;
    QAction* m_exportPlaylistAction = nullptr;
    QAction* m_helpAction = nullptr;
    QAction* m_quitAction = nullptr;
    QAction* m_prevAction = nullptr;
    QAction* m_nextAction = nullptr;
    QAction* m_firstAction = nullptr;
    QAction* m_lastAction = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_fitAction = nullptr;
    QAction* m_actualAction = nullptr;
    QAction* m_compareAction = nullptr;
    QAction* m_thumbsAction = nullptr;
    QAction* m_fullscreenAction = nullptr;
    QAction* m_specAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_aboutQtAction = nullptr;
    QActionGroup* m_langGroup = nullptr;
    QAction* m_langActions[3] = {nullptr, nullptr, nullptr};

    QMenu* m_fileMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_settingsMenu = nullptr;
    QMenu* m_langMenu = nullptr;
    QMenu* m_helpMenu = nullptr;
    QToolBar* m_toolbar = nullptr;

    QSpinBox* m_posSpin = nullptr;
    QLabel* m_countLabel = nullptr;
    QLabel* m_statusName = nullptr;
    QLabel* m_statusMeta = nullptr;
    QLabel* m_statusZoom = nullptr;
};

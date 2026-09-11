#include <QApplication>
#include <QComboBox>
#include <QListWidget>
#include <QtTest>

#include <memory>

#include "CompareView.h"
#include "ExportDialog.h"
#include "ImageStore.h"
#include "MainWindow.h"
#include "ThumbnailStrip.h"

namespace {
QString srcPath(const char* rel)
{
    return QString::fromUtf8(QTIV_SOURCE_DIR) + QLatin1Char('/') + QLatin1String(rel);
}
} // namespace

// GUI-тесты: запуск без дисплея (offscreen), регрессии на проводку виджетов.
class TestGui : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void thumbnailActivationSwitchesPhoto();
    void compareModeOpens();
    void compareThreePanels();
    void exportDialogFormatScope();

private:
    std::unique_ptr<MainWindow> m_window;
    ImageStore* m_store = nullptr;
};

void TestGui::initTestCase()
{
    m_window = std::make_unique<MainWindow>();
    QVERIFY(m_window->loadPaths({srcPath("examples/pattern.png"),
                                 srcPath("examples/mandelbrot.jpg")}));
    m_store = m_window->findChild<ImageStore*>();
    QVERIFY(m_store != nullptr);
    QCOMPARE(m_store->count(), 2);
    QCOMPARE(m_store->current(), 0);
}

void TestGui::thumbnailActivationSwitchesPhoto()
{
    ThumbnailStrip* strip = m_window->findChild<ThumbnailStrip*>();
    QVERIFY(strip != nullptr);

    // Активация миниатюры (клик или стрелки внутри ленты) — сигнал indexActivated —
    // должна переключать текущее фото в магазине.
    QVERIFY(QMetaObject::invokeMethod(strip, "indexActivated", Q_ARG(int, 1)));
    QCOMPARE(m_store->current(), 1);

    QVERIFY(QMetaObject::invokeMethod(strip, "indexActivated", Q_ARG(int, 0)));
    QCOMPARE(m_store->current(), 0);

    // Настоящий клик мышью по миниатюре: полный путь виджет → сигнал → стор.
    QListWidget* list = strip->findChild<QListWidget*>();
    QVERIFY(list != nullptr);
    const QRect rect = list->visualItemRect(list->item(1));
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
    QCOMPARE(m_store->current(), 1);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualItemRect(list->item(0)).center());
    QCOMPARE(m_store->current(), 0);
}

void TestGui::compareModeOpens()
{
    // Режим сравнения требует >= 2 фото и создаёт CompareView.
    QVERIFY(QMetaObject::invokeMethod(m_window.get(), "toggleCompare", Q_ARG(bool, true)));
    QVERIFY2(m_window->findChild<CompareView*>() != nullptr, "CompareView not created");
    QVERIFY(QMetaObject::invokeMethod(m_window.get(), "toggleCompare", Q_ARG(bool, false)));
}

void TestGui::compareThreePanels()
{
    // qtiv -comp -n=3 album.qtivp: демо-альбом (4 фото) → 3 панели.
    QVERIFY(m_window->loadPaths({srcPath("examples/demo.qtivp")}));
    QCOMPARE(m_window->playlistCount(), 4);

    m_window->showCompare({1, 3, 0});
    CompareView* view = m_window->findChild<CompareView*>();
    QVERIFY(view != nullptr);
    QCOMPARE(view->panelCount(), 3);
    QCOMPARE(view->panelIndex(0), 1);
    QCOMPARE(view->panelIndex(1), 3);
    QCOMPARE(view->panelIndex(2), 0);

    // Больше панелей, чем фото (8 > 4) — ограничивается количеством фото.
    m_window->showCompare({0, 1, 2, 3, 0, 1, 2, 3});
    view = m_window->findChild<CompareView*>();
    QVERIFY(view != nullptr);
    QCOMPARE(view->panelCount(), 4);

    QVERIFY(QMetaObject::invokeMethod(m_window.get(), "toggleCompare", Q_ARG(bool, false)));
}

void TestGui::exportDialogFormatScope()
{
    // Конвертация одного фото не предлагает альбом .qtivp:
    // альбом — это всегда весь плейлист (Ctrl+Shift+S).
    {
        ExportDialog dialog(m_store, ExportDialog::Scope::CurrentImage, m_window.get());
        QComboBox* combo = dialog.findChild<QComboBox*>();
        QVERIFY(combo != nullptr);
        QVERIFY2(combo->findData(QStringLiteral("qtivp")) < 0,
                 "single-image dialog must not offer .qtivp");
    }
    {
        ExportDialog dialog(m_store, ExportDialog::Scope::Playlist, m_window.get());
        QComboBox* combo = dialog.findChild<QComboBox*>();
        QVERIFY(combo != nullptr);
        QVERIFY2(combo->findData(QStringLiteral("qtivp")) >= 0,
                 "playlist dialog must offer .qtivp");
    }
}

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestGui tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_gui.moc"

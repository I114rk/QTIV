// qtiv-shot — генерация скриншотов приложения для документации и сайта.
// Запускается без экрана:
//   QT_QPA_PLATFORM=offscreen qtiv-shot main    out.png FILES...
//   QT_QPA_PLATFORM=offscreen qtiv-shot compare out.png FILES...

#include <QApplication>
#include <QStringList>
#include <QTimer>

#include <cstdio>

#include "Locale.h"
#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QTIV"));
    QApplication::setOrganizationName(QStringLiteral("QTIV"));

    if (argc < 3) {
        std::fprintf(stderr, "usage: qtiv-shot main|compare OUT.png [FILES...]\n");
        return 1;
    }
    const QString mode = QString::fromLocal8Bit(argv[1]);
    const QString out = QString::fromLocal8Bit(argv[2]);
    QStringList paths;
    for (int i = 3; i < argc; ++i)
        paths << QString::fromLocal8Bit(argv[i]);

    i18n::setLang(i18n::Lang::Russian);
    Theme::apply(app);

    MainWindow window;
    window.resize(1280, 800);
    if (!paths.isEmpty())
        window.loadPaths(paths);
    if (mode == QLatin1String("compare"))
        window.showCompare({0, 1, 2});
    window.show();

    // Даём ленте миниатюр сгенерироваться в фоне (таймер по 15 мс на кадр).
    QTimer::singleShot(700 + 150 * paths.size(), &app, [&window, out, &app] {
        window.grab().save(out);
        app.quit();
    });
    return app.exec();
}

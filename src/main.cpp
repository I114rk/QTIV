#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>

#include <cstdio>

#include "Cli.h"
#include "Locale.h"
#include "MainWindow.h"
#include "Settings.h"
#include "Theme.h"

namespace {

bool hasOption(int argc, char* argv[], const char* name)
{
    for (int i = 1; i < argc; ++i)
        if (qstrcmp(argv[i], name) == 0)
            return true;
    return false;
}

void printUsage()
{
    QTextStream out(stdout);
    out << "QTIV " QTIV_VERSION " — Qt Image Viewer\n"
        << "\n"
        << "Usage:\n"
        << "  qtiv [FILES...]            Open images, folders or .qtivp albums in the GUI\n"
        << "  qtiv -c, --cat FILES...    Show images directly in the terminal\n"
        << "      --ascii                Force ASCII art            (--cat mode)\n"
        << "      --half                 Force halfblock art        (--cat mode)\n"
        << "      --sixel                Force sixel graphics       (--cat mode)\n"
        << "      --kitty                Force kitty graphics       (--cat mode)\n"
        << "  qtiv -comp [FILES...]      Open in compare mode (2 panels)\n"
        << "  qtiv -comp -n=K FILE       Compare K photos from FILE (album or folder)\n"
        << "  qtiv -comp A.png B.png     Compare exactly these photos\n"
        << "      --info FILE            Print image/album info as JSON and exit\n"
        << "      --pack OUT FILES...    Pack images (or albums) into a .qtivp album\n"
        << "  -h, --help                 Show this help\n"
        << "  -v, --version              Show version\n"
        << "\n"
        << "Examples:\n"
        << "  qtiv photo.jpg             Folder of photo.jpg becomes the playlist\n"
        << "  qtiv a.jpg b.png c.webp    Playlist from exactly these files\n"
        << "  qtiv album.qtivp           Open album as playlist\n"
        << "  qtiv -c photo.png          Show photo in the terminal\n"
        << "  qtiv -comp before.jpg after.jpg\n"
        << "  qtiv -comp -n=3 album.qtivp\n"
        << "  qtiv --pack holiday.qtivp *.jpg\n"
        << "  qtiv --info album.qtivp\n";
    out.flush();
}

int runConsole(int argc, char* argv[])
{
    QString renderMode;
    QStringList catFiles;
    QString infoPath;
    QString packOut;
    QStringList packInputs;
    bool infoSet = false;
    bool packSet = false;
    bool catSet = false;

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);

        if (arg == QLatin1String("--info")) {
            infoSet = true;
        } else if (arg == QLatin1String("--pack")) {
            packSet = true;
        } else if (arg == QLatin1String("-c") || arg == QLatin1String("--cat")) {
            catSet = true;
        } else if (arg == QLatin1String("--ascii")) {
            renderMode = QStringLiteral("ascii");
        } else if (arg == QLatin1String("--half")) {
            renderMode = QStringLiteral("half");
        } else if (arg == QLatin1String("--sixel")) {
            renderMode = QStringLiteral("sixel");
        } else if (arg == QLatin1String("--kitty")) {
            renderMode = QStringLiteral("kitty");
        } else if (arg.startsWith(QLatin1Char('-'))) {
            // Остальные опции Qt (например, -platform) игнорируем.
        } else if (infoSet && infoPath.isEmpty()) {
            infoPath = arg;
        } else if (packSet && packOut.isEmpty()) {
            packOut = arg;
        } else if (packSet) {
            packInputs << arg;
        } else if (catSet) {
            catFiles << arg;
        }
    }

    if (infoSet) {
        if (infoPath.isEmpty()) {
            std::fprintf(stderr, "--info requires a file argument\n");
            return 2;
        }
        return Cli::info(infoPath);
    }
    if (packSet) {
        if (packOut.isEmpty()) {
            std::fprintf(stderr, "--pack requires an output file argument\n");
            return 2;
        }
        return Cli::pack(packOut, packInputs);
    }
    if (catSet)
        return Cli::cat(catFiles, renderMode);

    printUsage();
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    const bool consoleMode = hasOption(argc, argv, "--info")
        || hasOption(argc, argv, "--pack")
        || hasOption(argc, argv, "-c")
        || hasOption(argc, argv, "--cat")
        || hasOption(argc, argv, "--ascii")
        || hasOption(argc, argv, "--half")
        || hasOption(argc, argv, "--sixel")
        || hasOption(argc, argv, "--kitty")
        || hasOption(argc, argv, "-h")
        || hasOption(argc, argv, "--help")
        || hasOption(argc, argv, "-v")
        || hasOption(argc, argv, "--version");

    if (consoleMode) {
        if (hasOption(argc, argv, "-v") || hasOption(argc, argv, "--version")) {
            printf("QTIV %s\n", QTIV_VERSION);
            return 0;
        }
        if (hasOption(argc, argv, "-h") || hasOption(argc, argv, "--help")) {
            printUsage();
            return 0;
        }
        QCoreApplication app(argc, argv);
        return runConsole(argc, argv);
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QTIV"));
    QApplication::setApplicationVersion(QStringLiteral(QTIV_VERSION));
    QApplication::setOrganizationName(QStringLiteral("QTIV"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/qtiv.svg")));

    Theme::apply(app);
    i18n::setLang(static_cast<i18n::Lang>(AppSettings::instance().language()));

    MainWindow window;
    window.show();

    bool compareMode = false;
    int compareCount = -1; // -n=K: сколько фото сравнивать; -1 = не задано
    QStringList paths;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("-comp") || arg == QLatin1String("--compare")) {
            compareMode = true;
        } else if (arg.startsWith(QLatin1String("-n="))) {
            compareCount = arg.mid(3).toInt();
        } else if (arg.startsWith(QLatin1String("--count="))) {
            compareCount = arg.mid(8).toInt();
        } else if (arg.startsWith(QLatin1Char('-'))) {
            continue; // остальные опции Qt (например, -platform)
        } else if (arg.startsWith(QLatin1String("file://"))) {
            paths << QUrl(arg).toLocalFile();
        } else {
            paths << arg;
        }
    }
    if (compareCount > 0 && !compareMode)
        std::fprintf(stderr, "warning: -n=K has no effect without -comp\n");
    if (compareCount == 1)
        compareCount = 2;

    if (!paths.isEmpty())
        window.loadPaths(paths);

    if (compareMode && window.playlistCount() >= 2) {
        QVector<int> indices;
        const int total = window.playlistCount();
        if (paths.size() >= 2) {
            // Явный список файлов: сравниваем их (или первые K при -n=).
            const int n = compareCount > 0 ? qMin(compareCount, total) : total;
            for (int i = 0; i < n; ++i)
                indices << i;
            window.showCompare(indices);
        } else if (compareCount > 2) {
            // Один путь (альбом/папка) и -n=K: текущее фото и следующие.
            const int cur = window.currentIndex();
            for (int i = 0; i < compareCount; ++i)
                indices << (cur + i) % total;
            window.showCompare(indices);
        } else {
            // Один путь без -n=: пара «текущее + следующее».
            window.showCompare();
        }
    }

    return app.exec();
}

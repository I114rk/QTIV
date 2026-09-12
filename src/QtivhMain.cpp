// qtivh — QTIV Help: GUI-руководство по QTIV, консольным командам,
// формату .qtivp и инструменту qtivp. RU/EN, запускается standalone
// или из QTIV по F1.

#include <QApplication>

#include "HelpWindow.h"
#include "Locale.h"
#include "Settings.h"
#include "Theme.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QTIV"));
    QApplication::setApplicationVersion(QStringLiteral(QTIV_VERSION));
    QApplication::setOrganizationName(QStringLiteral("QTIV"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/qtiv.svg")));

    Theme::apply(app);
    i18n::setLang(static_cast<i18n::Lang>(AppSettings::instance().language()));

    HelpWindow help;
    help.show();
    return app.exec();
}

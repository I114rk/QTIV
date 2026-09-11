#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QPalette>

namespace {
QColor rgb(int r, int g, int b) { return QColor(r, g, b); }
} // namespace

void Theme::apply(QApplication& app)
{
    app.setStyle(QStringLiteral("Fusion"));

    QPalette pal;
    pal.setColor(QPalette::Window, rgb(0x1b, 0x1d, 0x23));
    pal.setColor(QPalette::WindowText, rgb(0xdf, 0xe3, 0xec));
    pal.setColor(QPalette::Base, rgb(0x22, 0x25, 0x2c));
    pal.setColor(QPalette::AlternateBase, rgb(0x26, 0x2a, 0x33));
    pal.setColor(QPalette::Text, rgb(0xdf, 0xe3, 0xec));
    pal.setColor(QPalette::Button, rgb(0x2a, 0x2e, 0x38));
    pal.setColor(QPalette::ButtonText, rgb(0xdf, 0xe3, 0xec));
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Highlight, rgb(0x4f, 0x8c, 0xff));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::ToolTipBase, rgb(0x26, 0x2a, 0x33));
    pal.setColor(QPalette::ToolTipText, rgb(0xe6, 0xe9, 0xf2));
    pal.setColor(QPalette::PlaceholderText, rgb(0x6b, 0x71, 0x80));
    pal.setColor(QPalette::Disabled, QPalette::Text, rgb(0x6b, 0x71, 0x80));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, rgb(0x6b, 0x71, 0x80));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, rgb(0x6b, 0x71, 0x80));
    app.setPalette(pal);

    QFile qss(QStringLiteral(":/assets/theme.qss"));
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
}

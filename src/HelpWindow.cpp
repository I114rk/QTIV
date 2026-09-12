#include "HelpWindow.h"
#include "Locale.h"
#include "Settings.h"

#include <QComboBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpWindow::HelpWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(QStringLiteral("QTIV — %1").arg(i18n::s("User guide")));
    resize(860, 680);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto* top = new QHBoxLayout;
    m_langLabel = new QLabel(this);
    m_langCombo = new QComboBox(this);
    m_langCombo->addItem(QStringLiteral("System"));
    m_langCombo->addItem(QStringLiteral("Русский"));
    m_langCombo->addItem(QStringLiteral("English"));
    const int saved = AppSettings::instance().language();
    m_langCombo->setCurrentIndex(saved >= 0 && saved < 3 ? saved : 0);
    connect(m_langCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) { applyLanguage(index); });

    top->addStretch(1);
    top->addWidget(m_langLabel);
    top->addWidget(m_langCombo);
    root->addLayout(top);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setStyleSheet(QStringLiteral(
        "QTextBrowser { background-color: #1b1d23; color: #dfe3ec; border: none; }"));
    root->addWidget(m_browser, 1);

    loadContent();
    retranslateUi();
}

void HelpWindow::applyLanguage(int lang)
{
    if (lang < 0 || lang > 2)
        return;
    AppSettings::instance().setLanguage(lang);
    i18n::setLang(static_cast<i18n::Lang>(lang));
    loadContent();
    retranslateUi();
    emit languageChanged();
}

void HelpWindow::loadContent()
{
    QString file = QStringLiteral(":/assets/help/ru.html");
    if (i18n::lang() == i18n::Lang::English)
        file = QStringLiteral(":/assets/help/en.html");
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_browser->setPlainText(i18n::s("Help file not found: %1").arg(file));
        return;
    }
    m_browser->setHtml(QString::fromUtf8(f.readAll()));
    m_browser->scrollToAnchor(QStringLiteral("top"));
}

void HelpWindow::retranslateUi()
{
    setWindowTitle(QStringLiteral("QTIV — %1").arg(i18n::s("User guide")));
    m_langLabel->setText(i18n::s("Language:"));
    m_langCombo->setItemText(0, i18n::s("System"));
}

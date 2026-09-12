#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QTextBrowser;

// Окно руководства qtivh: HTML-документ на русском или английском
// (assets/help/ru.html, en.html). Используется standalone-бинарником qtivh
// и открывается из QTIV по F1.
class HelpWindow : public QWidget {
    Q_OBJECT

public:
    explicit HelpWindow(QWidget* parent = nullptr);

    void retranslateUi();

signals:
    // Язык переключили в окне руководства — приложение тоже должно
    // перевести свой интерфейс.
    void languageChanged();

private:
    void loadContent();
    void applyLanguage(int lang);

    QTextBrowser* m_browser = nullptr;
    QComboBox* m_langCombo = nullptr;
    QLabel* m_langLabel = nullptr;
};

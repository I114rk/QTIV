#pragma once

#include <QString>

// Минималистичная локализация на два языка: английский — исходные строки
// (используются как ключи), русский — словарь в Locale.cpp.
// Переключение на лету: i18n::setLang() + retranslateUi() у виджетов.

namespace i18n {

enum class Lang { System = 0, Russian, English };

// Значение из настроек (без разрешения System).
Lang rawLang();

// Эффективный язык (System разрешается в Russian/English по локали системы).
Lang lang();

// Сохранить выбор (само по себе ничего не перерисовывает — вызывающий
// код должен вызвать retranslateUi()).
void setLang(Lang lang);

// Перевести английскую исходную строку. Отсутствие перевода = оригинал.
QString s(const QString& english);

// "3.8 МБ" / "3.8 MB" — с запятой/точкой по локали активного языка.
QString humanSize(qint64 bytes);

// Человекочитаемое имя языка для меню настроек.
QString langName(Lang lang);

} // namespace i18n

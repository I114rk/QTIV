#include "Locale.h"

#include <QHash>
#include <QLocale>

namespace i18n {
namespace {

Lang g_raw = Lang::System;
Lang g_effective = Lang::English;

// Русский словарь: ключ — английская исходная строка.
const QHash<QString, QString>& ru()
{
    static const QHash<QString, QString> dict = {
        // --- Сравнение -----------------------------------------------------
        {QStringLiteral("Image for this panel"), QStringLiteral("Изображение для этой панели")},
        {QStringLiteral("+ panel"), QStringLiteral("+ панель")},
        {QStringLiteral("\xE2\x88\x92 panel"), QStringLiteral("\xE2\x88\x92 панель")},
        {QStringLiteral("Add a comparison panel"), QStringLiteral("Добавить панель сравнения")},
        {QStringLiteral("Remove the last comparison panel"),
         QStringLiteral("Убрать последнюю панель сравнения")},
        {QStringLiteral("Close"), QStringLiteral("Закрыть")},
        {QStringLiteral("Image could not be decoded"), QStringLiteral("Не удалось декодировать изображение")},
        {QStringLiteral("Sync: on"), QStringLiteral("Синхр.: вкл")},
        {QStringLiteral("Sync: off"), QStringLiteral("Синхр.: выкл")},
        {QStringLiteral("Synchronize zoom and panning between panels (S)"),
         QStringLiteral("Синхронные зум и панорамирование панелей (S)")},
        {QStringLiteral("Nothing to show"), QStringLiteral("Нечего показать")},
        {QStringLiteral("—"), QStringLiteral("—")},

        // --- Руководство (qtivh) ------------------------------------------
        {QStringLiteral("User guide"), QStringLiteral("Руководство пользователя")},
        {QStringLiteral("Language:"), QStringLiteral("Язык:")},
        {QStringLiteral("Help file not found: %1"), QStringLiteral("Файл руководства не найден: %1")},
        {QStringLiteral("User &guide"), QStringLiteral("Руководство &пользователя")},
        {QStringLiteral("Open the user guide (F1)"), QStringLiteral("Открыть руководство (F1)")},

        // --- Экспорт -------------------------------------------------------
        {QStringLiteral("Browse…"), QStringLiteral("Обзор…")},
        {QStringLiteral("Destination folder:"), QStringLiteral("Папка назначения:")},
        {QStringLiteral("File name:"), QStringLiteral("Имя файла:")},
        {QStringLiteral("Format:"), QStringLiteral("Формат:")},
        {QStringLiteral("QTIVP album (photos in one file)"),
         QStringLiteral("Альбом QTIVP (несколько фото в одном файле)")},
        {QStringLiteral("Quality:"), QStringLiteral("Качество:")},
        {QStringLiteral("Cancel"), QStringLiteral("Отмена")},
        {QStringLiteral("Export"), QStringLiteral("Экспорт")},
        {QStringLiteral("Convert image"), QStringLiteral("Конвертация изображения")},
        {QStringLiteral("Export playlist"), QStringLiteral("Экспорт плейлиста")},
        {QStringLiteral("All photos from the playlist will be packed into one .qtivp album. Photos are stored without re-encoding."),
         QStringLiteral("Все фото из плейлиста будут упакованы в один альбом .qtivp. "
                        "Фото сохраняются без перекодирования.")},
        {QStringLiteral("The photo will be packed into a .qtivp album without re-encoding."),
         QStringLiteral("Фото будет упаковано в альбом .qtivp без перекодирования.")},
        {QStringLiteral("Each photo from the playlist will be converted and saved to the destination folder."),
         QStringLiteral("Каждое фото из плейлиста будет сконвертировано и сохранено в папку назначения.")},
        {QStringLiteral("Select destination folder"), QStringLiteral("Выберите папку назначения")},
        {QStringLiteral("File already exists. Overwrite?"), QStringLiteral("Файл уже существует. Перезаписать?")},
        {QStringLiteral("%1 files already exist. Overwrite?"),
         QStringLiteral("Файлов уже существует: %1. Перезаписать?")},
        {QStringLiteral("No image selected."), QStringLiteral("Изображение не выбрано.")},
        {QStringLiteral("Could not decode the image."), QStringLiteral("Не удалось декодировать изображение.")},
        {QStringLiteral("Saved: %1"), QStringLiteral("Сохранено: %1")},
        {QStringLiteral("Exported %1 files to %2"), QStringLiteral("Экспортировано файлов: %1 → %2")},
        {QStringLiteral("Exported %1 files, %2 failed:\n%3"),
         QStringLiteral("Экспортировано: %1, не удалось: %2:\n%3")},
        {QStringLiteral("Could not read the image data."), QStringLiteral("Не удалось прочитать данные изображения.")},
        {QStringLiteral("No readable photos to pack."), QStringLiteral("Нет читаемых фото для упаковки.")},
        {QStringLiteral("Packed %1 photos into %2"), QStringLiteral("Упаковано фото: %1 → %2")},

        // --- Главное окно ---------------------------------------------------
        {QStringLiteral("Open error"), QStringLiteral("Ошибка открытия")},
        {QStringLiteral("All files (*)"), QStringLiteral("Все файлы (*)")},
        {QStringLiteral("Open images"), QStringLiteral("Открыть изображения")},
        {QStringLiteral("Open .qtivp album"), QStringLiteral("Открыть альбом .qtivp")},
        {QStringLiteral("QTIVP albums (*.qtivp)"), QStringLiteral("Альбомы QTIVP (*.qtivp)")},
        {QStringLiteral("Extract album to folder"), QStringLiteral("Извлечь альбом в папку")},
        {QStringLiteral("Album error"), QStringLiteral("Ошибка альбома")},
        {QStringLiteral("Extracted %1 photos to %2"), QStringLiteral("Извлечено фото: %1 → %2")},
        {QStringLiteral("Failed:"), QStringLiteral("Ошибки:")},
        {QStringLiteral("Extract album"), QStringLiteral("Извлечение альбома")},
        {QStringLiteral("About the QTIVP format"), QStringLiteral("О формате QTIVP")},
        {QStringLiteral("QTIVP (.qtivp) is an open container for storing several photos in "
                        "one file: an 8-byte magic header, an entry index (name, MIME type, "
                        "dimensions, offsets, CRC32) and image data, optionally compressed "
                        "with zlib. Photos are stored without re-encoding.\n\n"
                        "Full specification: docs/QTIVP-SPEC.md in the QTIV repository."),
         QStringLiteral("QTIVP (.qtivp) — открытый контейнер для хранения нескольких фото "
                        "в одном файле: 8-байтовая сигнатура, индекс записей (имя, MIME-тип, "
                        "размеры, смещения, CRC32) и данные изображений с опциональным "
                        "сжатием zlib. Фото хранятся без перекодирования.\n\n"
                        "Полная спецификация: docs/QTIVP-SPEC.md в репозитории QTIV.")},
        {QStringLiteral("About QTIV"), QStringLiteral("О QTIV")},
        {QStringLiteral("Image viewer, comparison and conversion. Supports the open .qtivp album format."),
         QStringLiteral("Просмотр, сравнение и конвертация изображений. "
                        "Поддерживается открытый формат альбомов .qtivp.")},
        {QStringLiteral("License: MIT"), QStringLiteral("Лицензия: MIT")},
        {QStringLiteral("About Qt"), QStringLiteral("О Qt")},
        {QStringLiteral("Open an image or a folder (Ctrl+O)\nor drag and drop files here"),
         QStringLiteral("Откройте изображение или папку (Ctrl+O)\nили перетащите файлы сюда")},
        {QStringLiteral("Could not decode image"), QStringLiteral("Не удалось декодировать изображение")},
        {QStringLiteral("of %1"), QStringLiteral("из %1")},
        {QStringLiteral("No image"), QStringLiteral("Нет изображения")},

        // --- Действия и меню -------------------------------------------------
        {QStringLiteral("&Open…"), QStringLiteral("&Открыть…")},
        {QStringLiteral("Open images, a folder or a .qtivp album"),
         QStringLiteral("Открыть изображения, папку или альбом .qtivp")},
        {QStringLiteral("Open &album…"), QStringLiteral("Открыть &альбом…")},
        {QStringLiteral("Open a .qtivp album"), QStringLiteral("Открыть альбом .qtivp")},
        {QStringLiteral("&Extract album…"), QStringLiteral("&Извлечь альбом…")},
        {QStringLiteral("Save all photos from the current album to a folder"),
         QStringLiteral("Сохранить все фото текущего альбома в папку")},
        {QStringLiteral("&Convert image…"), QStringLiteral("&Конвертировать изображение…")},
        {QStringLiteral("Convert the current image to another format"),
         QStringLiteral("Конвертировать текущее изображение в другой формат")},
        {QStringLiteral("Export &playlist…"), QStringLiteral("Экспорт &плейлиста…")},
        {QStringLiteral("Convert all photos or pack them into a .qtivp album"),
         QStringLiteral("Конвертировать все фото или упаковать их в альбом .qtivp")},
        {QStringLiteral("&Quit"), QStringLiteral("В&ыход")},
        {QStringLiteral("&Previous"), QStringLiteral("&Предыдущее")},
        {QStringLiteral("&Next"), QStringLiteral("С&ледующее")},
        {QStringLiteral("&First image"), QStringLiteral("&Первое изображение")},
        {QStringLiteral("&Last image"), QStringLiteral("По&следнее изображение")},
        {QStringLiteral("Zoom &in"), QStringLiteral("У&величить")},
        {QStringLiteral("Zoom &out"), QStringLiteral("У&меньшить")},
        {QStringLiteral("Fit to &window"), QStringLiteral("По размеру &окна")},
        {QStringLiteral("Actual si&ze"), QStringLiteral("Реальный раз&мер")},
        {QStringLiteral("&Compare"), QStringLiteral("&Сравнение")},
        {QStringLiteral("Show two images side by side (sync zoom)"),
         QStringLiteral("Два изображения рядом (синхронный зум)")},
        {QStringLiteral("&Thumbnails"), QStringLiteral("&Миниатюры")},
        {QStringLiteral("Show or hide the thumbnail strip"),
         QStringLiteral("Показать или скрыть ленту миниатюр")},
        {QStringLiteral("&Fullscreen"), QStringLiteral("Полный &экран")},
        {QStringLiteral("QTIVP &format…"), QStringLiteral("&Формат QTIVP…")},
        {QStringLiteral("&About QTIV"), QStringLiteral("&О QTIV")},
        {QStringLiteral("About &Qt"), QStringLiteral("О &Qt")},
        {QStringLiteral("&File"), QStringLiteral("&Файл")},
        {QStringLiteral("&View"), QStringLiteral("&Вид")},
        {QStringLiteral("&Settings"), QStringLiteral("&Настройки")},
        {QStringLiteral("&Language"), QStringLiteral("&Язык")},
        {QStringLiteral("&Help"), QStringLiteral("&Справка")},
        {QStringLiteral("System"), QStringLiteral("Системный")},
        {QStringLiteral("Go to image number"), QStringLiteral("Перейти к изображению №")},
        {QStringLiteral("Images in the playlist"), QStringLiteral("Изображений в плейлисте")},
    };
    return dict;
}

Lang resolve(Lang lang)
{
    if (lang == Lang::System)
        return QLocale::system().language() == QLocale::Russian ? Lang::Russian : Lang::English;
    return lang;
}

} // namespace

Lang rawLang()
{
    return g_raw;
}

Lang lang()
{
    return g_effective;
}

void setLang(Lang lang)
{
    g_raw = lang;
    g_effective = resolve(lang);
}

QString s(const QString& english)
{
    if (g_effective == Lang::Russian) {
        const auto& dict = ru();
        const auto it = dict.constFind(english);
        if (it != dict.constEnd())
            return it.value();
    }
    return english;
}

QString humanSize(qint64 bytes)
{
    const char* unitsRu[] = { "Б", "КБ", "МБ", "ГБ" };
    const char* unitsEn[] = { "B", "KB", "MB", "GB" };
    const char** units = g_effective == Lang::Russian ? unitsRu : unitsEn;

    double value = double(bytes);
    int u = 0;
    while (value >= 1024.0 && u < 3) {
        value /= 1024.0;
        ++u;
    }
    const QLocale locale(g_effective == Lang::Russian ? QLocale::Russian : QLocale::English);
    const QString unit = QString::fromUtf8(units[u]);
    if (u == 0)
        return QStringLiteral("%1 %2").arg(locale.toString(qint64(value)), unit);
    return QStringLiteral("%1 %2").arg(locale.toString(value, 'f', 1), unit);
}

QString langName(Lang lang)
{
    switch (lang) {
    case Lang::Russian:
        return QStringLiteral("Русский");
    case Lang::English:
        return QStringLiteral("English");
    case Lang::System:
        break;
    }
    return QStringLiteral("System");
}

} // namespace i18n

cmake_minimum_required(VERSION 3.24)

project(qtiv VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# PIC обязателен: объекты ресурсов из статических библиотек ссылаются
# на защищённые символы данных Qt6Core (qt_resourceFeatureZstd).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

# Всё, кроме main.cpp: используется и приложением, и GUI-тестами.
add_library(qtiv_core STATIC
    src/Cli.h
    src/Cli.cpp
    src/Locale.h
    src/Locale.cpp
    src/Settings.h
    src/Settings.cpp
    src/Theme.h
    src/Theme.cpp
    src/Crc32.h
    src/Qtivp.h
    src/Qtivp.cpp
    src/TerminalRender.h
    src/TerminalRender.cpp
    src/ImageStore.h
    src/ImageStore.cpp
    src/ImageView.h
    src/ImageView.cpp
    src/ThumbnailStrip.h
    src/ThumbnailStrip.cpp
    src/CompareView.h
    src/CompareView.cpp
    src/ExportDialog.h
    src/ExportDialog.cpp
    src/HelpWindow.h
    src/HelpWindow.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)
target_include_directories(qtiv_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(qtiv_core PUBLIC QTIV_VERSION="${PROJECT_VERSION}")
target_link_libraries(qtiv_core PUBLIC Qt6::Widgets)

qt_add_executable(qtiv src/main.cpp)

# Ресурсы отдельной статической библиотекой: их используют qtiv, qtivh
# и GUI-тесты — все линкуются с qtiv_core.
add_library(qtiv_assets STATIC)
qt_add_resources(qtiv_assets PREFIX "/" FILES
    assets/theme.qss
    assets/qtiv.svg
    assets/icons/open.svg
    assets/icons/prev.svg
    assets/icons/next.svg
    assets/icons/zoom-in.svg
    assets/icons/zoom-out.svg
    assets/icons/fit.svg
    assets/icons/actual.svg
    assets/icons/compare.svg
    assets/icons/export.svg
    assets/icons/album.svg
    assets/icons/fullscreen.svg
    assets/icons/sync.svg
    assets/help/ru.html
    assets/help/en.html
)
target_link_libraries(qtiv_core PUBLIC qtiv_assets)

target_link_libraries(qtiv PRIVATE qtiv_core)

# Консольный инструмент для альбомов .qtivp (упаковка/распаковка/проверка).
qt_add_executable(qtivp-tool src/QtivpTool.cpp)
set_target_properties(qtivp-tool PROPERTIES OUTPUT_NAME qtivp)
target_link_libraries(qtivp-tool PRIVATE qtiv_core)

# GUI-руководство (qtivh).
qt_add_executable(qtivh src/QtivhMain.cpp)
target_link_libraries(qtivh PRIVATE qtiv_core)

include(GNUInstallDirs)

install(TARGETS qtiv qtivp-tool qtivh RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/qtiv.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES packaging/qtivh.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES packaging/x-qtivp.xml DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages)
install(FILES assets/qtiv.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)

# --- Tests ------------------------------------------------------------------
find_package(Qt6 COMPONENTS Test)
if(NOT Qt6Test_FOUND)
    message(STATUS "Qt6Test not found - unit tests skipped")
else()
    enable_testing()
    add_executable(tst_qtivp tests/tst_qtivp.cpp src/Qtivp.cpp src/Qtivp.h src/Crc32.h)
    target_include_directories(tst_qtivp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_qtivp PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME qtivp COMMAND tst_qtivp)

    add_executable(tst_gui tests/tst_gui.cpp)
    target_link_libraries(tst_gui PRIVATE qtiv_core Qt6::Test)
    target_compile_definitions(tst_gui PRIVATE QTIV_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME gui COMMAND tst_gui)

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME interop
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/qtivp_check.py
                         --bin $<TARGET_FILE:qtiv> --tool $<TARGET_FILE:qtivp-tool>
                         --workdir ${CMAKE_CURRENT_BINARY_DIR}/interop)
        add_test(NAME terminal
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pty_check.py
                         $<TARGET_FILE:qtiv>)
    else()
        message(STATUS "Python3 not found - interop test skipped")
    endif()
endif()

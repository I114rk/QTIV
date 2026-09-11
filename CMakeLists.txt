cmake_minimum_required(VERSION 3.24)

project(qtiv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(qtiv
    src/main.cpp
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
    src/MainWindow.h
    src/MainWindow.cpp
)

qt_add_resources(qtiv "assets" PREFIX "/" FILES
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
)

target_compile_definitions(qtiv PRIVATE QTIV_VERSION="${PROJECT_VERSION}")

target_link_libraries(qtiv PRIVATE Qt6::Widgets)

include(GNUInstallDirs)

install(TARGETS qtiv RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/qtiv.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
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

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME interop
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/qtivp_check.py
                         --bin $<TARGET_FILE:qtiv> --workdir ${CMAKE_CURRENT_BINARY_DIR}/interop)
    else()
        message(STATUS "Python3 not found - interop test skipped")
    endif()
endif()

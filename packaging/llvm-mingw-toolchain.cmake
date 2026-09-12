# Кросс-компиляция QTIV под Windows x86_64.
# Тулчейн: llvm-mingw (ucrt, libc++) — https://github.com/mstorsjo/llvm-mingw
# Qt: официальный сборка llvm-mingw (Clang) той же версии, что и хост-инструменты.
#
# Пример:
#   cmake -B build-win -GNinja -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_TOOLCHAIN_FILE=packaging/llvm-mingw-toolchain.cmake \
#     -DQTIV_MINGW_ROOT=$HOME/llvm-mingw \
#     -DQTIV_QT_ROOT=$HOME/qt-win/6.11.2/<поддиректория> \
#     -DQT_HOST_PATH=/usr

if(NOT QTIV_MINGW_ROOT OR NOT QTIV_QT_ROOT)
    message(FATAL_ERROR
        "Задайте -DQTIV_MINGW_ROOT=<llvm-mingw> и -DQTIV_QT_ROOT=<qt для windows>")
endif()

# Переменные должны попадать и в try_compile (проверки ABI компилятора),
# иначе тулчейн-файл там не найдёт компиляторы.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES QTIV_MINGW_ROOT QTIV_QT_ROOT)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER   ${QTIV_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${QTIV_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER  ${QTIV_MINGW_ROOT}/bin/x86_64-w64-mingw32-windres)
set(CMAKE_AR           ${QTIV_MINGW_ROOT}/bin/x86_64-w64-mingw32-ar CACHE FILEPATH "")
set(CMAKE_RANLIB       ${QTIV_MINGW_ROOT}/bin/x86_64-w64-mingw32-ranlib CACHE FILEPATH "")

list(APPEND CMAKE_FIND_ROOT_PATH ${QTIV_MINGW_ROOT} ${QTIV_QT_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

cmake_minimum_required(VERSION 3.10)
project(SudokuCode LANGUAGES CXX)

# Prefer clang on all platforms; fall back to system C++ compiler if absent.
if (NOT DEFINED ENV{CXX} AND NOT DEFINED CMAKE_CXX_COMPILER)
    find_program(CLANG_CXX NAMES clang++ clang-c++ HINTS ENV PATH)
    if (CLANG_CXX)
        set(CMAKE_CXX_COMPILER "${CLANG_CXX}" CACHE FILEPATH "C++ compiler" FORCE)
        set(CMAKE_C_COMPILER "${CLANG_CXX}" CACHE FILEPATH "C compiler" FORCE)
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core library sources
set(SUDOKU_SRCS src/sudokucode.cpp)
set(SUDOKU_HDRS src/sudokucode.h)

# ---------- Tests ----------
option(BUILD_TESTS "Build test suite" OFF)
if (BUILD_TESTS)
    add_executable(sudokucode_test tests/test.cpp ${SUDOKU_SRCS})
    target_include_directories(sudokucode_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                       ${CMAKE_CURRENT_SOURCE_DIR}/src)
    enable_testing()
    add_test(NAME sudokucode_roundtrip COMMAND sudokucode_test)
endif ()

# ---------- Qt GUI (optional) ----------
option(BUILD_GUI "Build Qt GUI" OFF)
if (BUILD_GUI)
    set(GUI_SRCS
        gui/mainwindow.cpp
        gui/gridwidget.cpp
        gui/gui_main.cpp
    )
    set(GUI_HDRS
        gui/mainwindow.h
        gui/gridwidget.h
    )

    # Prefer Qt6, fall back to Qt5
    find_package(Qt6 COMPONENTS Widgets Gui QUIET _qt6)
    if (_qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
        set(QT_WIDGETS Qt6::Widgets)
        set(QT_GUI Qt6::Gui)
    else()
        find_package(Qt5 COMPONENTS Widgets Gui REQUIRED)
        set(QT_VERSION_MAJOR 5)
        set(QT_WIDGETS Qt5::Widgets)
        set(QT_GUI Qt5::Gui)
    endif()

    add_executable(sudokucode_gui ${GUI_SRCS} ${GUI_HDRS} ${SUDOKU_SRCS})
    target_include_directories(sudokucode_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                       ${CMAKE_CURRENT_SOURCE_DIR}/src
                                                       ${CMAKE_CURRENT_SOURCE_DIR}/gui)
    target_link_libraries(sudokucode_gui PRIVATE ${QT_WIDGETS} ${QT_GUI})

    if (QT_VERSION_MAJOR EQUAL 6)
        set_target_properties(sudokucode_gui PROPERTIES CXX_STANDARD 17)
    endif()

    install(TARGETS sudokucode_gui RUNTIME DESTINATION bin)
endif ()

if (BUILD_TESTS)
    install(TARGETS sudokucode_test RUNTIME DESTINATION bin)
endif ()
install(FILES README.md DESTINATION share/sudokucode)

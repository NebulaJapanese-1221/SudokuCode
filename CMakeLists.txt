cmake_minimum_required(VERSION 3.10)
project(SudokuCode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sudokucode main.cpp sudokucode.cpp)
target_include_directories(sudokucode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Optional test suite
option(BUILD_TESTS "Build test suite" OFF)
if (BUILD_TESTS)
    add_executable(sudokucode_test test.cpp sudokucode.cpp)
    target_include_directories(sudokucode_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME sudokucode_roundtrip COMMAND sudokucode_test)
endif ()

install(TARGETS sudokucode RUNTIME DESTINATION bin)
install(FILES README.md DESTINATION share/sudokucode)

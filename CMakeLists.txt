cmake_minimum_required(VERSION 3.20)

project(TextAnalyzerLab7 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(text_analyzer STATIC
    src/PolynomialSolver.cpp
    src/TextAnalyzer.cpp
)
target_include_directories(text_analyzer
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(text_analyzer PUBLIC POLYNOMIAL_SOLVER_STATIC)

if(MSVC)
    target_compile_options(text_analyzer PRIVATE /W4 /permissive-)
else()
    target_compile_options(text_analyzer PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(text_analyzer_demo src/main.cpp)
target_link_libraries(text_analyzer_demo PRIVATE text_analyzer)

add_executable(bad_exception_demo src/BadExceptions.cpp)

include(CTest)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)

    add_executable(text_analyzer_tests tests/TextAnalyzerTests.cpp)
    target_link_libraries(text_analyzer_tests PRIVATE text_analyzer GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(text_analyzer_tests)
endif()

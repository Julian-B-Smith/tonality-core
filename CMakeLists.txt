cmake_minimum_required(VERSION 3.20)
project(tonality-core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tonality INTERFACE)
target_include_directories(tonality INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(emit_table tools/emit_table.cpp)
target_link_libraries(emit_table PRIVATE tonality)

enable_testing()

set(TONALITY_FIXTURES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tonality)

add_executable(parity_table tests/parity_table.cpp)
target_link_libraries(parity_table PRIVATE tonality)
target_compile_definitions(parity_table PRIVATE
    TONALITY_FIXTURES_DIR="${TONALITY_FIXTURES_DIR}")
add_test(NAME parity_table COMMAND parity_table)

add_executable(parity_conformance tests/parity_conformance.cpp)
target_link_libraries(parity_conformance PRIVATE tonality)
target_include_directories(parity_conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_compile_definitions(parity_conformance PRIVATE
    TONALITY_FIXTURES_DIR="${TONALITY_FIXTURES_DIR}")
add_test(NAME parity_conformance COMMAND parity_conformance)

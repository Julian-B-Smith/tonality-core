cmake_minimum_required(VERSION 3.20)
project(tonality-core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tonality INTERFACE)
target_include_directories(tonality INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Byte-for-byte parity with the CPython engine requires Python-identical
# floating-point arithmetic: FMA contraction (clang's default) fuses a*b±c
# into one rounding and diverges the reflection_residual minimizer by an ulp
# — enough to flip an achiral set's ±0.0. Contraction is forbidden for every
# consumer of these headers.
target_compile_options(tonality INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-ffp-contract=off>)

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

# Optional pybind11 fast path (Decision 10: an addition, never a replacement).
# Configure with -DTONALITY_BUILD_PYTHON=ON and point Python_EXECUTABLE at the
# interpreter that has pybind11 installed (e.g. the Tonality venv).
option(TONALITY_BUILD_PYTHON "Build the pybind11 fast-path module" OFF)
if(TONALITY_BUILD_PYTHON)
    find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
    if(NOT pybind11_DIR)
        execute_process(
            COMMAND ${Python_EXECUTABLE} -m pybind11 --cmakedir
            OUTPUT_VARIABLE pybind11_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY)
    endif()
    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(tonality_core bindings/tonality_core_py.cpp)
    target_link_libraries(tonality_core PRIVATE tonality)
    add_test(NAME parity_bindings
        COMMAND ${Python_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity_bindings.py
                ${TONALITY_FIXTURES_DIR}
                $<TARGET_FILE_DIR:tonality_core>)
endif()

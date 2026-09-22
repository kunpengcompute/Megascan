cmake_minimum_required(VERSION 3.16)

project(megascan_fuzz C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MS_FUZZ_ENABLE_SANITIZERS "Enable ASan and UBSan" ON)
option(MS_FUZZ_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)
set(MS_FUZZ_MIN_FILE_COVERAGE "80" CACHE STRING
    "Required line coverage percentage for every ms/src file")
set(MS_FUZZ_LCOV_IGNORE_ERRORS "" CACHE STRING
    "Optional lcov/genhtml --ignore-errors value, for example inconsistent,negative")

get_filename_component(MS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_subdirectory("${MS_ROOT}/ms" ms-build)

add_executable(megascan_fuzz
    main.cpp
    fuzz_test.cpp
    data/data_generator.cpp
    generator/ms_generator.cpp
    runner/ms_runner.cpp
)

target_include_directories(megascan_fuzz PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MS_ROOT}/ms/include"
)
target_link_libraries(megascan_fuzz PRIVATE ms)

if(MSVC)
    target_compile_options(megascan_fuzz PRIVATE /W4 /WX)
else()
    target_compile_options(megascan_fuzz PRIVATE -Wall -Wextra -Werror)
endif()

if(MS_FUZZ_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CMakePushCheckState)
    include(CheckCSourceCompiles)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_c_source_compiles("int main(void) { return 0; }"
                           MS_FUZZ_SANITIZERS_AVAILABLE)
    cmake_pop_check_state()

    if(MS_FUZZ_SANITIZERS_AVAILABLE)
        set(SANITIZER_FLAGS -fsanitize=address,undefined
                            -fno-omit-frame-pointer)
        target_compile_options(ms PRIVATE ${SANITIZER_FLAGS})
        target_link_options(ms PRIVATE ${SANITIZER_FLAGS})
        target_compile_options(megascan_fuzz PRIVATE ${SANITIZER_FLAGS})
        target_link_options(megascan_fuzz PRIVATE ${SANITIZER_FLAGS})
    else()
        message(WARNING
            "ASan/UBSan runtime libraries are unavailable; building fuzz "
            "without sanitizers. Install the compiler sanitizer runtimes or "
            "configure with -DMS_FUZZ_ENABLE_SANITIZERS=OFF to silence this warning.")
    endif()
endif()

if(MS_FUZZ_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # glibc rejects _FORTIFY_SOURCE at -O0. The ms target enables
    # _FORTIFY_SOURCE=2 and treats warnings as errors, so keep lightweight
    # optimization while retaining useful source-level coverage data.
    target_compile_options(ms PRIVATE -Og -g --coverage
                                      -Wno-error=maybe-uninitialized)
    target_link_options(ms PRIVATE --coverage)
    target_compile_options(megascan_fuzz PRIVATE -Og -g --coverage)
    target_link_options(megascan_fuzz PRIVATE --coverage)

    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    if(NOT LCOV_EXECUTABLE OR NOT GENHTML_EXECUTABLE)
        message(FATAL_ERROR
            "MS_FUZZ_ENABLE_COVERAGE=ON requires both lcov and genhtml")
    endif()

    set(MS_FUZZ_COVERAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/coverage")
    set(MS_FUZZ_COVERAGE_INFO
        "${MS_FUZZ_COVERAGE_DIR}/coverage.info")
    set(MS_FUZZ_COVERAGE_HTML
        "${MS_FUZZ_COVERAGE_DIR}/html")
    set(MS_FUZZ_LCOV_IGNORE_ARGS)
    set(MS_FUZZ_GENHTML_IGNORE_ARGS)
    if(MS_FUZZ_LCOV_IGNORE_ERRORS)
        list(APPEND MS_FUZZ_LCOV_IGNORE_ARGS
             --ignore-errors "${MS_FUZZ_LCOV_IGNORE_ERRORS}")
        list(APPEND MS_FUZZ_GENHTML_IGNORE_ARGS
             --ignore-errors "${MS_FUZZ_LCOV_IGNORE_ERRORS}")
    endif()

    add_custom_target(coverage_reset
        COMMAND ${LCOV_EXECUTABLE} --zerocounters
                --directory "${CMAKE_BINARY_DIR}"
        DEPENDS megascan_fuzz
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(coverage_report
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${MS_FUZZ_COVERAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MS_FUZZ_COVERAGE_DIR}"
        COMMAND ${LCOV_EXECUTABLE} --capture
                --directory "${CMAKE_BINARY_DIR}"
                --output-file "${MS_FUZZ_COVERAGE_DIR}/coverage-all.info"
                --rc geninfo_unexecuted_blocks=1
                ${MS_FUZZ_LCOV_IGNORE_ARGS}
        COMMAND ${LCOV_EXECUTABLE} --extract
                "${MS_FUZZ_COVERAGE_DIR}/coverage-all.info"
                "${MS_ROOT}/ms/src/*"
                --output-file "${MS_FUZZ_COVERAGE_INFO}"
                ${MS_FUZZ_LCOV_IGNORE_ARGS}
        COMMAND ${LCOV_EXECUTABLE} --remove
                "${MS_FUZZ_COVERAGE_INFO}"
                "${MS_ROOT}/ms/src/expr/ms_expr_scan_stream.c"
                --output-file "${MS_FUZZ_COVERAGE_INFO}"
                ${MS_FUZZ_LCOV_IGNORE_ARGS}
        COMMAND ${CMAKE_COMMAND}
                -DTRACEFILE=${MS_FUZZ_COVERAGE_INFO}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/filter_public_api_coverage.cmake"
        COMMAND ${GENHTML_EXECUTABLE}
                "${MS_FUZZ_COVERAGE_INFO}"
                --output-directory "${MS_FUZZ_COVERAGE_HTML}"
                ${MS_FUZZ_GENHTML_IGNORE_ARGS}
                --title "Megascan fuzz coverage"
        COMMAND ${CMAKE_COMMAND} -E echo
                "Coverage report: ${MS_FUZZ_COVERAGE_HTML}/index.html"
        DEPENDS megascan_fuzz
        USES_TERMINAL
        VERBATIM
    )
endif()

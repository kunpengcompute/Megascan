set(target megascan_demo)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../output/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../output
)

add_executable(${target}
    api_demo.c
    demo_common.c
    allocator_demo.c
    handler_demo.c
    expression_demo.c
    database_demo.c
    logical_demo.c
    api_test_demo.c
    benchmark_case_demo.c
    benchmark_runner.c
    data_generator_runner.c
    workflow_runner.cpp
    workflow_results.cpp
)

target_include_directories(${target}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ms/include
)

target_link_libraries(${target}
    PRIVATE
    ms
    pthread
)

set_target_properties(${target}
    PROPERTIES
    BUILD_RPATH "$ORIGIN/../../../output/lib;$ORIGIN/../lib"
)

set(target megascan_demo)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../output/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../output
)

add_executable(${target}
    api_demo.c
    demo_common.c
    flow_demo.c
    api_cli_demo.c
    api_catalog_demo.c
    api_session_demo.c
    api_session_extended_demo.c
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

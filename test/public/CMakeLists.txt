set(target test_main)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../unpack_open_source/googletest/lib)

if (${USE_TEST_FRAMEWORK} STREQUAL CUNIT)
file(GLOB_RECURSE src_files ${CMAKE_CURRENT_SOURCE_DIR}/*c)
else()
file(GLOB_RECURSE src_files ${CMAKE_CURRENT_SOURCE_DIR}/*cpp)
endif()

message("src_files" ${src_files})

add_compile_options(-g)

add_library(${target} STATIC ${src_files} main.c ktest_interactive.c)

if (${USE_TEST_FRAMEWORK} STREQUAL CUNIT)
target_link_libraries(${target} PUBLIC pthread cunit)
else()
target_link_libraries(${target} PUBLIC pthread gtest)
endif()

message("CMAKE_CURRENT_SOURCE_DIR:${CMAKE_CURRENT_SOURCE_DIR}")
message("PROJECT_SOURCE_DIR:${PROJECT_SOURCE_DIR}")
message("PROJECT_INCLUDE_DIR:${PROJECT_INCLUDE_DIR}")
target_include_directories(${target}
    PRIVATE
    "${PROJECT_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

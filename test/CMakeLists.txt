cmake_minimum_required(VERSION 3.10)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)
set(CMAKE_CXX_STANDARD 14)

# 设置测试框架采用CUNIT/GTEST
set(USE_TEST_FRAMWORK CUNIT)

# 设置依赖头文件路径
set(PROJECT_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/public/include
    ${CMAKE_CURRENT_SOURCE_DIR}/public/cunit/include
    ${CMAKE_CURRENT_SOURCE_DIR}/public/cunit/
)

#设置需要连接库路径
set(PROJECT_LINK_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/public/cunit/lib
)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS
    "-march=armv8-a+crc \
    -g \
    -Wall \
    -Wdate-time \
    -Wextra"
)
add_link_options(--coverage)

LINK_DIRECTORIES(${PROJECT_LINK_DIR})

# coverage option
if(COVER)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fprofile-arcs -ftest-coverage -fno-access-control")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fprofile-arcs -ftest-coverage")
    message("CMAKE_CXX_FLAGS:${CMAKE_CXX_FLAGS}")
endif()

add_subdirectory(public)
add_subdirectory(src)
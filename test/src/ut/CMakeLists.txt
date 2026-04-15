#设置测试二进制名称
set(target testdemo_ut)

LINK_DIRECTORIES(
    ${PROJECT_LINK_DIR}
)

FILE(GLOB testcode_source ${CMAKE_CURRENT_SOURCE_DIR}/*.c ${CMAKE_CU})

message("sec_driver_source:" ${sec_driver_source})

add_executable(${target} ${testcode_source})

#设置测试码需要测试的源码头文件目录
target_include_directories(${target}
    PUBLIC
    ${PROJECT_INCLUDE_DIR}
)

#设置测试码需要连接的库
target_link_libraries(${target} PUBLIC
    "-Wl,--whole-archive"
    test_main
    "-Wl,--no-whole-archive"
    cunit
)
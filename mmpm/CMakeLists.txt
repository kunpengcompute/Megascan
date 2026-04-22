add_library(mmpm SHARED
    src/mmpm.c
    src/mmpm_hash.c
    src/mmpm_ac.c
)
target_link_libraries(mmpm PRIVATE securec)

target_include_directories(mmpm PRIVATE
    include
    ${CMAKE_SOURCE_DIR}/opensource/huawei_secure_c/include
)

target_compile_options(mmpm PRIVATE -Wall -Werror -fvisibility=hidden -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fpie -fpic -ftrapv)

target_link_options(mmpm PRIVATE -s -Wl,-z,relro,-z,now,-z,noexecstack -pie)

# 设置输出路径
set_target_properties(mmpm
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/lib
)

execute_process(
    COMMAND sh -c "mkdir -p ${CMAKE_SOURCE_DIR}/output/include/ && cp -rf ./include/mmpm.h ${CMAKE_SOURCE_DIR}/output/include/"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/mmpm
)
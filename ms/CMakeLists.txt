add_library(ms SHARED
    src/ms.c
    src/ms_hash.c
    src/ms_ac.c
    src/ms_allocator.c
)

target_include_directories(ms PRIVATE
    include
)

target_compile_options(ms PRIVATE -Wall -Werror -fvisibility=hidden -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fpie -fpic -ftrapv)

target_link_options(ms PRIVATE -s -Wl,-z,relro,-z,now,-z,noexecstack -pie)

# 设置输出路径
set_target_properties(ms
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/lib
)

add_custom_command(TARGET ms POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/output/include
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ms.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ms_allocator.h
            ${CMAKE_SOURCE_DIR}/output/include
)

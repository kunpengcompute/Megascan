add_library(mmpm SHARED
    src/mmpm.c
    src/mmpm_rule.c
    src/expr/mmpm_expr_compile.c
    src/expr/mmpm_expr_context.c
    src/expr/mmpm_expr_runtime.c
    src/expr/mmpm_expr_runtime_util.c
    src/expr/mmpm_expr_scan_block.c
    src/expr/mmpm_expr_scan_stream.c
    src/mmpm_hash.c
    src/mmpm_ac.c
    src/mmpm_stream.c
    src/mmpm_logical.c
    src/mmpm_allocator.c
    src/mmpm_database.c
    src/mmpm_scratch.c
)

target_include_directories(mmpm PRIVATE
    include
    src/expr
)

target_compile_options(mmpm PRIVATE -Wall -Werror -fvisibility=hidden -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fpie -fpic -ftrapv)

target_link_options(mmpm PRIVATE -s -Wl,-z,relro,-z,now,-z,noexecstack -pie)

# 设置输出路径
set_target_properties(mmpm
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output/lib
)

add_custom_command(TARGET mmpm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/output/include
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm_common.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm_rule.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm_compile.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm_runtime.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm_expr.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm_logical.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm_allocator.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/mmpm_database.h
            ${CMAKE_SOURCE_DIR}/output/include
)

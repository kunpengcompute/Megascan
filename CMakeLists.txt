cmake_minimum_required(VERSION 3.22.0)
project(DADK C)

# import library
add_library(securec STATIC IMPORTED)
set_target_properties(securec PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/opensource/huawei_secure_c/lib/libsecurec.a
)

add_subdirectory(mmpm)

set(CMAKE_INSTALL_PREFIX '')
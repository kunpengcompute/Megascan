#!/bin/bash
 ##############################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# Build mmpm RPM package
##############################################################
set -eu

script_path="${BASH_SOURCE[0]}"
workspace_dir=$(cd $(dirname "$script_path") && pwd)

function build_huawei_secure_c()
{
    if [ -f "$workspace_dir/opensource/huawei_secure_c/lib/libsecurec.a" ]; then
        return
    fi
    
    if [ ! -d "$workspace_dir/opensource/huawei_secure_c" ]; then
        echo "can't find the ${workspace_dir}/opensource/huawei_secure_c"
        exit 1
    fi

    cd $workspace_dir/opensource/huawei_secure_c/src
    make lib
    if [ ! -f "$workspace_dir/opensource/huawei_secure_c/lib/libsecurec.a" ]; then
        echo "compile huawei_secure_c static lib failed."
        exit 1
    fi
}

function build_mmpm()
{
    # compile
    cd $workspace_dir
    echo "clean"
    rm -rf build
    cmake -S . -B build

    cmake --build build # install to ./output
    
    cd ./output
    tar -czvf mmpm.tar.gz lib/ include/
    mkdir -p rpmbuild/SOURCES
    mv mmpm.tar.gz rpmbuild/SOURCES

    rpmbuild --define "_topdir $workspace_dir/output/rpmbuild" -ba $workspace_dir/spec/mmpm.spec
    mv rpmbuild/RPMS/aarch64/*.rpm .
    
    rm -rf include/ lib/
    rm -rf rpmbuild
}

build_huawei_secure_c

build_mmpm

exit 0
#!/bin/bash
 ##############################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# Build mmpm RPM package
##############################################################
set -eu

script_path="${BASH_SOURCE[0]}"
workspace_dir=$(cd $(dirname "$script_path") && pwd)

function build_mmpm()
{
    # compile
    cd $workspace_dir
    echo "clean"
    rm -rf build
    cmake -S . -B build

    cmake --build build # install to ./output
    
    mkdir -p output/lib
    cp ./output/lib/libmmpm.so ./output/
    
    cd ./output
    tar -czvf mmpm.tar.gz lib/ include/
    mkdir -p rpmbuild/SOURCES
    mv mmpm.tar.gz rpmbuild/SOURCES

    rpmbuild --define "_topdir $workspace_dir/output/rpmbuild" -ba $workspace_dir/spec/mmpm.spec
    mv rpmbuild/RPMS/aarch64/*.rpm .
    
    rm -rf include/ lib/
    rm -rf rpmbuild
}

build_mmpm

exit 0
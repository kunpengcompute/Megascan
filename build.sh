#!/bin/bash
 ##############################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# Build ms RPM package
##############################################################
set -eu

script_path="${BASH_SOURCE[0]}"
workspace_dir=$(cd $(dirname "$script_path") && pwd)

function build_ms()
{
    # compile
    cd $workspace_dir
    echo "clean"
    rm -rf build
    cmake -S . -B build

    cmake --build build # install to ./output
    
    cd ./output
    tar -czvf ms.tar.gz lib/ include/
    mkdir -p rpmbuild/SOURCES
    mv ms.tar.gz rpmbuild/SOURCES

    rpmbuild --define "_topdir $workspace_dir/output/rpmbuild" -ba $workspace_dir/spec/ms.spec
    mv rpmbuild/RPMS/aarch64/*.rpm .
    
    rm -rf include/ lib/
    rm -rf rpmbuild
}

build_ms

exit 0

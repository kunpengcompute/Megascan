#!/bin/bash 
 ##############################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# excute Unit test
##############################################################
set -e
 
workdir="$(pwd)"
 
function sec_driver_test()
{
    cd $workdir/test
    python3 build.py cover
 
    echo "success to execute Unit test for mmpm."
}
 
sec_driver_test
 
exit 0
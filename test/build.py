#!/usr/bin/env python
##############################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# coding=utf-8
# Python script for building.
##############################################################
import os
import sys
import shlex, subprocess
import shutil
import time

path = os.path.abspath(os.path.dirname(__file__))
build_path = path + "/build"

def log(info):
    print(info)

def clean_func():
    if os.path.exists(build_path):
        shutil.rmtree(build_path)
    log("clean down")
    exit()

def build_func(target, build_param):
    src_path = path
    if os.path.exists(build_path):
        shutil.rmtree(build_path)
    os.mkdir(build_path)
    os.chdir(build_path)
    cmake_param = "cmake .." + build_param
    log(cmake_param)
    subprocess.run(cmake_param, shell=True)
    subprocess.run("make " + target, shell=True)
    os.chdir(src_path)
    if not os.listdir(build_path + "/bin"):
        log("build failed")
        raise Exception("build failed")
        return -1
    log("build testcode success")
    

def compile_src(target = "all", type = "release", extra = ""):
    """
    call os cmd to build src
    compile options type: release debug clean
                   extra: cover asan tsan
    """

    param_sheet = {"release":"", "debug":" -DDEBUG=ON", "cover":" -DCOVER=ON",
                    "asan":" -DASAN=ON", "tsan":" -DTSAN=ON", "all":"", "":""}

    log("***************start build***************")

    if (type == "clean"):
        clean_func()
        return
    build_param = param_sheet[type] + param_sheet[extra]
    if build_func(target, build_param) != -1 and (extra == "cover" or extra == "run"):
        compile_done()

def start_compile(user_arg):
    """
    user cmd deal
    """
    log("use: ./build.py [all/target] [release/debug/clean] [cover/asan/tsan]")
    param = {1:"all", 2:"release", 3:""}

    type_sheet = ["release", "debug", "clean"]
    extra_sheet = ["cover", "asan", "tsan", "all", ""]

    if (len(user_arg) > 4):
        log("use: ./build.py [all/target] [release/debug/clean] [cover/asan/tsan]")
        exit()

    start_num = 0

    if (len(user_arg) > 1):
        if (user_arg[1] in type_sheet):
            start_num = 2
        elif (user_arg[1] in extra_sheet):
            start_num = 3
        else:
            start_num = 1

    for i in range(start_num, start_num + len(user_arg) - 1):
        param[i] = user_arg[i - start_num + 1]

    target = param[1]
    type = param[2]
    extra = param[3]

    print(target, type, extra)

    if (type in type_sheet and extra in extra_sheet):
        return (target, type, extra)
    else:
        log("use: ./build.py [all/target] [release/debug/clean] [cover/asan/tsan/run]")
        exit()

def compile_done():
    proc = subprocess.run("bash ./script/lcov_test.sh", shell=True, stdout=subprocess.PIPE)
    print(proc.stdout.decode("utf-8"))
    ret = proc.stdout.decode("utf-8").splitlines()
    for i in range(0, len(ret)):
        if "Run Summary" in ret[i]:
            break
    test_info = ret[i + 2].split()
    test_cost = ret[i + 5].split()[3]
    xml_list = ["<?xml version='1.0' encoding='UTF-8'?>",
               '<testsuits tests="%s" failures="%s" disabled="%s" errors="%s" timestamp="%s" time="%s" name="ALLTest"/>' % (
               test_info[1], test_info[4], test_info[5], test_info[4], time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()), test_cost)]
    xml_str = "\n".join(xml_list)
    with open("%s/build/test_result.xml" % path, 'w+') as fp:
        fp.write(xml_str)

if __name__ == "__main__":
    (target, type, extra) = start_compile(sys.argv)
    compile_src(target, type, extra)
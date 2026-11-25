#!/bin/bash
##############################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# CI unit test for Megascan:
#   1) 安装 build 作业产出的 RPM（缺失则兜底本地构建）
#   2) python3 build.py 构建 UT
#   3) 运行 UT 并解析 CUnit Run Summary 判定结果
##############################################################
set -eu

workspace_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "${workspace_dir}"

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
elif command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
else
    echo "ERROR: 安装 RPM 需要 root 权限"
    exit 1
fi

# 1) 准备主库：优先使用 build 制品，缺失时兜底本地构建
if ! ls output/*.aarch64.rpm >/dev/null 2>&1; then
    echo "WARN: 未找到 build 制品 RPM，兜底本地构建"
    bash build.sh
fi

${SUDO} rpm -Uvh --replacepkgs output/*.aarch64.rpm
${SUDO} ldconfig
test -f /usr/local/lib/libms.so

# 2) 构建 UT（LIBRARY_PATH 供链接，LD_LIBRARY_PATH 供运行加载）
export LIBRARY_PATH="/usr/local/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
export LD_LIBRARY_PATH="/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd test
python3 build.py testdemo_ut debug

# 3) 运行 UT。入口固定 return 0，不能只看退出码，须解析 Run Summary
set +e
test_log=$(./build/bin/testdemo_ut 2>&1)
run_ret=$?
set -e
echo "${test_log}"
if [ "${run_ret}" -ne 0 ]; then
    echo "ERROR: testdemo_ut 异常退出，退出码 ${run_ret}"
    exit 1
fi

summary=$(printf '%s\n' "${test_log}" | sed -n '/Run Summary/,$p')
tests_line=$(printf '%s\n' "${summary}" | awk '$1=="tests"{print; exit}')
suites_line=$(printf '%s\n' "${summary}" | awk '$1=="suites"{print; exit}')
asserts_line=$(printf '%s\n' "${summary}" | awk '$1=="asserts"{print; exit}')

if [ -z "${tests_line}" ]; then
    echo "ERROR: 未找到 CUnit Run Summary 的 tests 行，UT 结果无法判定"
    exit 1
fi
if [ -z "${suites_line}" ] || [ -z "${asserts_line}" ]; then
    echo "ERROR: Run Summary 缺少 suites/asserts 行，UT 结果无法判定"
    exit 1
fi

read -r _ t_total t_ran t_passed t_failed t_inactive <<< "${tests_line}"
read -r _ _ _ _ s_failed _ <<< "${suites_line}"
read -r _ _ _ _ a_failed _ <<< "${asserts_line}"

if [ "${t_total}" -le 0 ]; then
    echo "ERROR: tests 总数为 ${t_total}，未实际执行用例"
    exit 1
fi
if [ "${t_failed}" -ne 0 ] || [ "${t_inactive}" -ne 0 ]; then
    echo "ERROR: 用例失败 ${t_failed} 个、未执行 ${t_inactive} 个"
    exit 1
fi
if [ "${s_failed}" -ne 0 ] || [ "${a_failed}" -ne 0 ]; then
    echo "ERROR: suites 失败 ${s_failed} 个、asserts 失败 ${a_failed} 个"
    exit 1
fi

echo "ut success: tests=${t_total}, passed=${t_passed}, failed=${t_failed}, inactive=${t_inactive}"

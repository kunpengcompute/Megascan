#!/bin/bash
##############################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# CI build for Megascan: 使用仓库自带 build.sh 编译主库并打包 RPM
##############################################################
set -eu

workspace_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "${workspace_dir}"

bash build.sh

rpm_file=$(ls -1 output/*.aarch64.rpm | head -n 1)
test -s "${rpm_file}"
echo "build success: ${rpm_file}"

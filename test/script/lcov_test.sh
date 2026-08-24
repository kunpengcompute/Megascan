#!/bin/bash
##############################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# Generate ut coverage report
##############################################################
set -e

current_path=$(cd $(dirname $0); pwd)
#指定源码所在目录
# src_path=${current_path}/../../../vdml/
# src_path=${current_path}/../../../build/vdml
src_path=${current_path}/../build/src/ut/
outpath=${current_path}/../build/cover
mkdir -p ${outpath}
for file in $(ls ${current_path}/../build/bin/); do
	${current_path}/../build/bin/${file}
done
echo "   "
find $src_path -name '*.gcda'
echo "   "
testinfo=${outpath}/test.info
lcov -d $src_path -t 'test' -o ${testinfo} -b . -c --rc lcov_branch_coverage=1
lcov --remove ${testinfo} \
        '*/8/*' \
        '*/7/*' \
        '*/include/*' \
        '*/build/*' \
        '*/tests/*' \
        -o ${testinfo} --rc lcov_branch_coverage=1 \
        --ignore-errors unused
echo "   "
genhtml -o ${outpath}/lcov_result ${testinfo} --branch-coverage
echo "Please check ${outpath}/lcov_result !!!"

exit 0

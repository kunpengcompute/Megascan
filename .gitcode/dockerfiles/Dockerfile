FROM swr.cn-north-4.myhuaweicloud.com/cloud_boostkit/openeuler-24.03-lts-sp3:latest

# ---- 0. 包管理器与 pip 镜像源 ----
ENV PIP_INDEX_URL=https://repo.huaweicloud.com/repository/pypi/simple \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

RUN printf '[global]\nindex-url = https://repo.huaweicloud.com/repository/pypi/simple\ntimeout = 120\n' > /etc/pip.conf

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    TZ=Asia/Shanghai

# sh 指向 bash：CI 脚本通过 sh 调用且含 bash 特性
# git：checkout action 必需；rpm-build/tar：build.sh 打包 RPM；CUnit-devel：UT 链接与头文件
RUN ln -sf /bin/bash /bin/sh && \
    dnf makecache && dnf install -y \
        gcc gcc-c++ cmake make python3 CUnit-devel git rpm-build rpm tar gzip wget && \
    dnf clean all

WORKDIR /workspace

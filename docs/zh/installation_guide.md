# 安装指南<a name="ZH-CN_TOPIC_0000002553346771"></a>

本文介绍Megascan的源码编译和RPM安装方法。源码编译适用于开发、调试和自定义构建；RPM适用于已获取正式软件包的部署场景。

为保证您可以顺利安全地使用Megascan，请确保所使用的环境信息在已验证环境范围内。若您在使用过程中遇到问题，建议先检查使用的环境能否切换为已验证的环境范围。

**表 1** 已验证环境<a id="已验证环境"></a>

|操作系统|CPU类型|GCC版本|
|--|--|--|
|openEuler 22.03 LTS SP4|鲲鹏920新型号处理器|10.3.1|
|openEuler 24.03 LTS SP1|鲲鹏920新型号处理器|12.3.1|


编译Megascan动态库不依赖Boost、PCRE等第三方算法库。

|软件|版本|用途|
|--|--|--|
|GCC|10.3及以上|必选，编译Megascan动态库|
|CMake|3.22及以上|必选，生成构建工程|
|GNU Make|操作系统配套版本|必选，执行默认构建|
|glibc及POSIX Threads|操作系统配套版本|必选，提供基础运行库和线程接口|
|Python 3|3.8及以上|可选，构建测试程序|
|CUnit|1.0及以上|可选，运行单元测试|
|rpm-build、tar|操作系统配套版本|可选，构建RPM软件包|

在openEuler上可安装基础编译工具：

```bash
sudo dnf install -y gcc gcc-c++ cmake make
```

安装后检查版本：

```bash
gcc --version
cmake --version
make --version
```

## 源码编译

以下命令均在Megascan源码根目录执行。

1. 进入源码目录。

    ```bash
    cd Megascan
    ```

2. 生成Release构建目录。

    ```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    ```

3. 编译动态库。

    ```bash
    cmake --build build -j"$(nproc)"
    ```

4. 检查编译产物。

    ```bash
    test -f output/lib/libms.so && echo "libms.so: OK"
    test -f output/include/ms.h && echo "ms.h: OK"
    ```

    回显示例：

    ```text
    libms.so: OK
    ms.h: OK
    ```

编译完成后的主要产物如下：

|路径|说明|
|--|--|
|`output/lib/libms.so`|Megascan动态库|
|`output/include/*.h`|对外头文件|
|`build/`|CMake中间文件|

需要保留调试符号和栈帧信息时，可单独创建构建目录：

```bash
cmake -S . -B build-profile \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DMS_ENABLE_PROFILING=ON
cmake --build build-profile -j"$(nproc)"
```

重新构建时，删除对应构建目录后再次执行CMake命令即可：

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## 编译并运行测试

测试程序依赖Python 3和CUnit。在openEuler上可先安装：

```bash
sudo dnf install -y python3 CUnit-devel
```

如果环境中没有`CUnit-devel`软件包，请参见[《KTest测试框架》](../../test/README.md)，将CUnit头文件和库放入测试工程。

完成Megascan动态库编译后，在源码根目录执行：

```bash
cd test
python3 build.py debug
```

运行单元测试：

```bash
./build/bin/testdemo_ut
```

查看demo帮助：

```bash
./build/bin/megascan_demo --help
```

此时所在目录为`Megascan/test`，因此两个程序的实际生成位置分别为`Megascan/test/build/bin/testdemo_ut`和`Megascan/test/build/bin/megascan_demo`。KTest说明、测试通过条件和覆盖率方法请参见[《KTest测试框架》](../../test/README.md)，demo的完整使用方法请参见[《Demo使用说明》](../../test/demo/README.md)。

## 安装RPM软件包

已获取正式RPM软件包时，可直接安装：

```bash
sudo rpm -ivh boostida-kdadk-ms-release-xxxx.aarch64.rpm
```

其中`xxxx`表示软件版本。安装完成后，默认文件位置如下：

|路径|说明|
|--|--|
|`/usr/local/lib/libms.so`|Megascan动态库|
|`/usr/local/include/*.h`|对外头文件|

检查安装结果：

```bash
rpm -q boostkit-kdadk-ms
rpm -ql boostkit-kdadk-ms
ldconfig -p | grep libms
```

安装成功的回显示例如下，版本号和头文件列表以实际软件包为准：

```text
boostkit-kdadk-ms-1.1.0-oe2203.aarch64
/usr/local/lib/libms.so
/usr/local/include/ms.h
...
libms.so (libc6,AArch64) => /usr/local/lib/libms.so
```

升级已安装的软件包时使用：

```bash
sudo rpm -Uvh boostida-kdadk-ms-release-xxxx.aarch64.rpm
```

升级成功的回显示例如下，软件包名称和进度以实际结果为准：

```text
Verifying...                          ################################# [100%]
Preparing...                          ################################# [100%]
Updating / installing...
   1:boostkit-kdadk-ms-xxxx           ################################# [ 50%]
Cleaning up / removing...
   2:boostkit-kdadk-ms-xxxx           ################################# [100%]
```

## 构建RPM软件包

`build.sh`用于在AArch64环境构建RPM，执行前需要安装打包工具：

```bash
sudo dnf install -y rpm-build tar
```

安装成功后，`dnf`命令末尾通常显示：

```text
Complete!
```

在源码根目录执行：

```bash
chmod +x build.sh
./build.sh
```

生成的软件包位于`output`目录，可执行以下命令确认：

```bash
ls -1 output/*.rpm
```

回显示例：

```text
output/boostida-kdadk-ms-release-1.1.0.oe2203.aarch64.rpm
```

该脚本会重新构建源码并整理打包目录，不建议用于仅需要`libms.so`的日常增量编译。

## 验证开发环境

完成源码编译或RPM安装后，可按照[快速入门](./quick_start.md)编译示例程序。示例能够成功链接`libms.so`并输出预期规则ID，表示Megascan开发环境可用。

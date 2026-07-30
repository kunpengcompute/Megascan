### KTest测试框架：

#### 1、整体介绍

KTest测试框架内部支持gtest、cunit两种测试框架，用户可在`test/CMakeLists.txt`中指定具体采用何种框架。当前Megascan使用cunit。

具体编写测试码，参考：https://codehub-y.huawei.com/Kunpeng-Computing-DC/Intelligent-NIC/Intelligent-NIC-SDK/Network-Acc/wiki/view/doc/699869。

用户使用框架时可修改项均在`test/CMakeLists.txt`中通过注释指出，用户可参考修改。

#### 2、编译

前提：

1. 首先保证源码已编译完成。
2. openEuler可通过`sudo dnf install -y python3 CUnit-devel`安装测试依赖。
3. 如果环境中没有`CUnit-devel`软件包，可将[cunit.tar.gz](https://rnd-isourceb.huawei.com/images/DG/20210812/79f91d50-d54e-4a16-a225-250ec4e0128a/cunit.tar.gz)拷贝到`test/public/cunit`目录并解压。

    ```text
    python3 build.py [target] [release/debug/clean] [cover/asan/tsan]
    cover：覆盖率
    asan：内存检测（暂未开启）
    tsan：线程竞争检测（暂未开启）
    ```

    `target`不指定时默认编译全部目标，也可指定`testdemo_ut`或`megascan_demo`。

```bash
cd test
python3 build.py debug
```

#### 3、运行

```bash
cd ./build/bin/
./testdemo_ut
```

#### 4、覆盖率

保证源码先编译完成，并安装`lcov`和`genhtml`。返回`Megascan/test`目录后执行：

```bash
python3 build.py testdemo_ut debug cover
```

报告生成位置为`build/cover/lcov_result`目录，可打开其中的`index.html`查看覆盖率报告。

#### 5、运行demo

返回`Megascan/test`目录后执行：

```bash
./build/bin/megascan_demo --help
```

API参数化调用、session流程和920/HG测试方法请参见[《Demo使用说明》](./demo/README.md)。

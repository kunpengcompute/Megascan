### KTest测试框架：
#### 1、整体介绍
KTest测试框架内部支持（gtest、cunit）两种测试框架，用户可在最外层CMakelists.txt中指定具体采用何种框架
具体编写测试码，参考:https://codehub-y.huawei.com/Kunpeng-Computing-DC/Intelligent-NIC/Intelligent-NIC-SDK/Network-Acc/wiki/view/doc/699869。
用户使用框架时可修改项均在CMakelists.txt中通过注释指出，用户可参考修改
#### 2、编译
前提：
1、首先保证源码已编译完成
2、拷贝压缩包[cunit.tar.gz](https://rnd-isourceb.huawei.com/images/DG/20210812/79f91d50-d54e-4a16-a225-250ec4e0128a/cunit.tar.gz)到test/public/cunit下
3、解压test/public/cunit下的cunit.tar.gz(由于目前开源库引进目录未定，采用此种方式，后续需要更改)
```c
python build.py [all/target(for example:hwoff_ut)] [release/debug/clean] [cover/asan/tsan/run]
cover:覆盖率
asan:内存检测（暂未开启）
tsan:线程竞争检测（暂未开启）
run:编译后自动运行（暂未开启）
```
```c
cd test/
./build.py debug
```
#### 3、运行
```c
cd ./build/bin/
./hwoff_ut
```
若需要以交互形式运行
```c
./hwoff_ut -i（暂不支持）
```

#### 4、覆盖率
保证源码先编译完成（需带覆盖率编译选项）
```c
运行测试码：
cd test
python3 debug cover
```
报告生成位置为：build/cover/lcov_result目录下，可将整个目录拷贝至pc上查看网页形式index.html报告
![image.png](https://rnd-isourceb.huawei.com/images/SH/20210825/64178385-f254-44da-804a-3475a53b0a38/image.png)
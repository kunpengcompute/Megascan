### KTest测试框架：
#### 1、整体介绍
KTest测试框架支持gtest和CUnit两种测试框架，用户可在最外层CMakeLists.txt中指定具体采用何种框架。
测试代码的编写方式可参考仓库中的现有测试用例以及gtest或CUnit的公开文档。
用户可修改的配置项已在CMakeLists.txt中通过注释标出。
#### 2、编译
前提：
1、首先保证源码已编译完成
2、从CUnit公开发布渠道获取源码包，并将编译生成的头文件和库文件分别放入`test/public/cunit/include`和`test/public/cunit/lib`目录。
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
报告生成位置为`build/cover/lcov_result`目录，可通过浏览器打开其中的`index.html`查看报告。

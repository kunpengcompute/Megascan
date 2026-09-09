# Megascan 本地 Fuzz 测试

该工具是单机、进程内的确定性随机测试器，不依赖集群、消息队列或网络服务。
它覆盖 AC database、兼容 handler、stream、HASH 和 logical API，并默认启用
AddressSanitizer 与 UndefinedBehaviorSanitizer。

## 构建

在 Linux 单机环境执行：

```bash
cd test/fuzz
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

配置阶段会检测 sanitizer 是否能成功链接。若系统未安装 `libasan` 或
`libubsan`，构建会给出警告并自动关闭 sanitizer。也可显式关闭该检测和警告：

```bash
cmake -S . -B build -DMS_FUZZ_ENABLE_SANITIZERS=OFF
```

## 运行

默认串行执行 1000 轮，固定默认 seed，便于稳定复现：

```bash
./build/megascan_fuzz
```

单机多核并行示例：

```bash
./build/megascan_fuzz --rounds 100000 --threads 4
```

这里的 `--threads` 仅创建本进程内的工作线程。生成器仍按 round 顺序生成用例，
每个工作线程持有独立 Runner。失败时程序会输出 seed、round 和可直接执行的串行
复现命令。

查看全部参数：

```bash
./build/megascan_fuzz --help
```

## Coverage 报告

Coverage 与 sanitizer 相互独立。最小化系统没有 ASan/UBSan 运行库时，仍可关闭
sanitizer 并使用 GCC coverage、lcov 和 genhtml 生成 HTML 报告。Coverage 构建使用
`-Og`，以兼容 Megascan 启用的 `_FORTIFY_SOURCE=2`：

```bash
cmake -S . -B build-coverage \
  -DCMAKE_BUILD_TYPE=Debug \
  -DMS_FUZZ_ENABLE_SANITIZERS=OFF \
  -DMS_FUZZ_ENABLE_COVERAGE=ON
cmake --build build-coverage --parallel
cmake --build build-coverage --target coverage_reset
./build-coverage/megascan_fuzz --duration-seconds 600 --threads 1 --report-only
cmake --build build-coverage --target coverage_report
```

构建、运行和生成报告是三个独立步骤，普通构建不会自动运行 fuzz。

报告入口为：

```text
build-coverage/coverage/html/index.html
```

配置 coverage 前需安装 `lcov`（其中包含 `genhtml`）。`coverage_reset` 清零旧计数，
`coverage_report` 只收集现有计数并生成 HTML。`--report-only` 会记录第一个 API
异常及异常总数，但不会因普通 API 断言失败提前终止；直接运行
`megascan_fuzz` 时仍保持失败即退出。

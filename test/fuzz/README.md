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

要求每个 `libms.so` 导出接口都至少调用 3000 万次时，使用单线程接口计数模式：

```bash
./build/megascan_fuzz \
  --calls-per-api 30000000 \
  --threads 1 \
  --report-only
```

该模式按接口轮询：每个用例只指定一个目标对外接口，并按该接口需要的正常流程
创建、编译和释放依赖对象。统计中的 `calls` 是目标接口测试次数，`helpers` 是该
接口作为其他目标的初始化、查询或清理依赖时产生的额外真实调用次数。终止条件
只看 `calls`，因此辅助调用不会造成某接口虚假达到 3000 万次；未达到目标的接口
会继续被调度。

每分钟会输出目标次数最少的接口、速率和预计剩余秒数；结束时输出全部 44 个
导出接口的目标次数、辅助次数、失败次数和目标调用速率。`--calls-per-api`
使用精确停止条件，因此固定要求 `--threads 1`。

按时间运行时可以使用多线程。调度器仍按导出接口轮询设置目标 API，但用例数据、
规则、flag 和生命周期状态继续由 seed 与 round 生成：

```bash
./build/megascan_fuzz --duration-seconds 600 --threads 4 --report-only
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
./build-coverage/megascan_fuzz --duration-seconds 600 --threads 4 --report-only
cmake --build build-coverage --target coverage_report
```

覆盖率运行混合随机数据、确定性重叠匹配、边界匹配、无匹配、QUIET、
SINGLEMATCH、AND/OR 组合以及分块 stream 场景。公开 API 还会覆盖常见负向状态，
例如 NULL 参数、非法 matcher type、重复 init/destroy、空对象生命周期、输出
buffer/capacity 不匹配和未初始化对象调用。

覆盖率报告只保留公开 API 可达的 `ms/src` 实现。`ms_expr_scan_stream.c` 当前没有
公开 API 调用链进入；公开 stream wrapper 走 `ms_scratch.c` 的 workspace 路径。
因此主 fuzz 不直接调用该内部文件，也不把它计入公开 API 覆盖率验收。

接口次数与覆盖率可以在同一份 coverage 计数上分两阶段累计：

```bash
cmake --build build-coverage --target coverage_reset
./build-coverage/megascan_fuzz --duration-seconds 600 --threads 4 --report-only
./build-coverage/megascan_fuzz --calls-per-api 30000000 --threads 1 --report-only
cmake --build build-coverage --target coverage_report
```

构建、运行和生成报告是三个独立步骤，普通构建不会自动运行 fuzz。

报告入口为：

```text
build-coverage/coverage/html/index.html
```

`coverage_report` 会逐个检查 `ms/src` 下进入 lcov 的源码文件，默认要求每个文件
行覆盖率不低于 80%。HTML 会先生成；若有文件不达标，命令返回失败并逐项打印
文件名、覆盖行数和百分比。阈值可在配置时通过
`-DMS_FUZZ_MIN_FILE_COVERAGE=80` 指定。

配置 coverage 前需安装 `lcov`（其中包含 `genhtml`）。`coverage_reset` 清零旧计数，
`coverage_report` 只收集现有计数并生成 HTML。`--report-only` 会记录第一个 API
异常及异常总数，但不会因普通 API 断言失败提前终止；直接运行
`megascan_fuzz` 时仍保持失败即退出。

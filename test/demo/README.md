# Megascan Demo
`megascan_demo` 是基于 Megascan 库构建的命令行示例应用，包含两类能力：
1. 调用 Megascan 公开接口，演示规则编译、块扫描、流扫描、HASH 查询和逻辑组合。
2. 执行完整 920/HG 测试，包括数据生成、单核、整机、quick、profile、汇总和跨机器比较。
功能演示与正式性能测试使用不同入口：
- `scan`、`lookup`、`corpus`、`logical` 用于接口验证和问题复现。
- `920hg single`、`920hg full` 用于正式性能基线。
- `benchmark` 用于手动复现单个 case，不作为跨机器基线入口。

## 文件说明
- `api_demo.c`：命令行入口、中文帮助、场景和公开接口索引。
- `demo_common.c`：场景公共选项、数据结构和结果输出。
- `database_demo.c`：database、scratch、块扫描和流扫描示例。
- `handler_demo.c`：handler、HASH 和 legacy AC 示例。
- `expression_demo.c`：表达式与规则接口示例。
- `logical_demo.c`：逻辑状态设置与组合求值示例。
- `allocator_demo.c`：自定义 allocator 设置与恢复示例。
- `benchmark_case_demo.c`：读取规则和 corpus，执行正确性扫描。
- `benchmark_runner.c`：将 `bench_920hg.c` 接入 `benchmark`。
- `data_generator_runner.c`：将 `gen_920hg_data.c` 接入 `generate-data`。
- `workflow_runner.cpp`：920hg 数据、构建、测试矩阵和 profile 流程。
- `workflow_results.cpp`：原始 CSV 汇总、模式比较和 920/HG 对比。
- `920hg/bench_920hg.c`：正式测试唯一的扫描、预热、计时和吞吐量实现。
- `920hg/gen_920hg_data.c`：规则和 corpus 的确定性生成核心。

## 构建
在 `Megascan/test` 目录执行：
```sh
cd /path/to/Megascan/test
python3 build.py megascan_demo debug
```
生成文件：
```text
test/build/bin/megascan_demo
```
后续示例统一使用：
```sh
DEMO=./build/bin/megascan_demo
```
执行 `920hg single/full` 时，demo 会另外构建 Release Megascan，并按固定参数单独编译正式 benchmark。

## 帮助
```sh
$DEMO -h
$DEMO scan -h
$DEMO lookup -h
$DEMO corpus -h
$DEMO benchmark -h
$DEMO generate-data -h
$DEMO logical -h
$DEMO 920hg -h
$DEMO 920hg generate -h
$DEMO 920hg single -h
$DEMO 920hg full -h
$DEMO 920hg quick -h
$DEMO 920hg profile -h
$DEMO 920hg summarize -h
$DEMO 920hg compare-single -h
```
不带参数运行时进入交互菜单。每个具体命令和 920hg 子命令均支持独立中文帮助。

## 接口调用示例
查看场景、公开接口，或运行指定接口对应的场景：
```sh
$DEMO scenarios
$DEMO apis
$DEMO --api ms_scan_database
$DEMO --scenario stream-audit --input-file request.dat --chunk-size 64
$DEMO --all
```
扫描文本、流式扫描文件、HASH 查询和逻辑求值：
```sh
$DEMO scan --matcher ac --mode block \
  --pattern 'token=' --input 'user=alice&token=abc'
$DEMO scan --matcher accombo --mode stream-full \
  --rules-file rules.tsv --rule-count 50000 \
  --input-file request.dat --chunk-size 64
$DEMO lookup --key malware-sha256-demo
$DEMO logical --expression '101&!102' --set '101=1,102=0'
```
matcher 支持 `ac`、`accombo`、`ac_legacy`、`ac_on_combo`，扫描模式支持 `block` 和 `stream-full`。HASH 规则是固定 16 字节 tuple；AC/ACCombo 使用 `ID<TAB>flags<TAB>expression` TSV。上述命令用于接口验证，不统计正式吞吐量。

## 使用 920hg 数据验证接口
设置已有数据目录：
```sh
DATA=/home/data/Megascan/920hg_data_v4
```
复现一条普通 AC block 查询：
```sh
$DEMO corpus --algorithm ac --mode block \
  --rules-file "$DATA/shared/ac_50000_rules.tsv" \
  --rule-count 50000 \
  --corpus-file "$DATA/shared/ac_q200000_p560_h5000.bin" \
  --query-index 0
```
复现 ACCombo stream 查询：
```sh
$DEMO corpus --algorithm accombo --mode stream-full \
  --rules-file "$DATA/shared/accombo_50000_rules.tsv" \
  --rule-count 50000 \
  --corpus-file "$DATA/shared/accombo_q200000_p560_h5000.bin" \
  --query-index 0
```
扫描完整 corpus：
```sh
$DEMO corpus --algorithm accombo --mode stream-full \
  --rules-file "$DATA/shared/accombo_50000_rules.tsv" \
  --rule-count 50000 \
  --corpus-file "$DATA/shared/accombo_q200000_p560_h5000.bin" \
  --all-queries
```
`corpus` 会校验 corpus 保存的期望命中信息，但不执行完整测试矩阵，也不输出正式吞吐量。

## 手动 Benchmark
```sh
$DEMO benchmark -a ac -M both -n 50000 \
  -t "$DATA/shared/ac_50000_rules.tsv" \
  -s "$DATA/shared/ac_q200000_p560_h5000.bin" \
  -p ./ac_raw.csv -T local -R 1 -w 0 -c 1
```
主要参数：
- `-a`：`hash`、`ac`、`accombo`、`ac_legacy`、`ac_on_combo`。
- `-M`：`lookup`、`block`、`stream-full`、`both`。
- `-n`：规则数量；`-t`：规则文件；`-s`：corpus。
- `-p`：原始 CSV；`-T`：机器标识。
- `-R`：trial；`-w`：warmup；`-c`：worker CPU 列表。
该命令适合手动复现单个 case。它使用 demo 可执行文件中的 benchmark 核心，不能与正式独立 benchmark 的数据混用。

## 920hg 统一入口
```sh
$DEMO 920hg <操作> [选项]
```
也兼容 `$DEMO --920hg <操作>`。
920hg 的数据矩阵、构建、调度、CPU/NUMA 控制、汇总和比较均由 demo 内部完成，不调用旧 Shell/Python 测试脚本。
正式 benchmark 仍编译为独立进程，以保持固定编译参数、进程隔离、CPU affinity 和历史计时口径。

## 目录约定
默认工作目录：
```text
${XDG_CACHE_HOME:-$HOME/.cache}/megascan/920hg
```
默认数据与结果：
```text
$WORK_ROOT/data
$WORK_ROOT/results/<类型>/<机器>
```
可以设置环境变量：
```sh
export MEGASCAN_ROOT=/path/to/Megascan
export WORK_ROOT=/data/megascan-work
export DATA_ROOT=/data/920hg_data_v4
```
也可使用 `--work-root`、`--data-root` 和 `--out-dir` 逐次指定。

## 生成完整数据
```sh
$DEMO 920hg generate --data-root /data/920hg_data_v4
```
默认矩阵：
- HASH 规则数：`5000000 10000000 50000000 90000000`。
- AC/ACCombo 规则数：`50000 100000 500000 900000`。
- HASH 命中基点：`100 500 1000 5000 9000 10000`。
- AC 命中基点：`100 1000 5000 9000`。
- query 数量：`200000`。
- AC payload：`560` 字节。
- ACCombo 比例：`10%`。
- seed：`9202026`。
命中基点范围为 `0..10000`，`5000` 表示 50%。
自定义矩阵：
```sh
$DEMO 920hg generate \
  --data-root /data/920hg_small \
  --hash-rule-counts '5000000' \
  --ac-rule-counts '50000 100000' \
  --hash-hit-bps '5000' \
  --ac-hit-bps '1000 5000' \
  --query-count 200000 --payload-size 560 \
  --combo-percent 10 --seed 9202026
```
相同配置可重复执行并复用已有文件。若目录已有不同 `suite_config.env`，命令会拒绝覆盖。
数据由固定 seed 确定生成；同一算法和命中率的 corpus 在不同规则规模间复用。920 与 HG 必须使用同一份数据，不能分别生成后直接比较。
主要文件：
- `suite_config.env`：数据配置。
- `manifest.csv`：全部 case 与文件路径。
- `shared/hash_*_rules.bin`：HASH 规则。
- `shared/ac_*_rules.tsv`：普通 AC 规则。
- `shared/accombo_*_rules.tsv`：ACCombo 规则。
- `shared/*corpus*.bin`：query 与期望结果。
- `hash/`、`ac/`、`accombo/`：case metadata。

## 单核正式测试
```sh
$DEMO 920hg single --machine 920 \
  --data-root /data/920hg_data_v4 --cpu-core 1
$DEMO 920hg single --machine hg \
  --data-root /data/920hg_data_v4 --cpu-core 1
```
默认行为：
- `trials=1`、`warmups=0`。
- 使用 CPU 1，禁止 CPU 0。
- 运行 HASH、AC、AC legacy、ACCombo、AC-on-Combo。
- AC 类算法同时执行 block 与 stream-full。
- 全新构建 Release Megascan。
- 使用 `gcc -O2 -DNDEBUG` 单独编译 `bench_920hg.c`。
- 自动汇总并生成模式和算法对比 CSV。
指定输出目录：
```sh
$DEMO 920hg single --machine 920 \
  --data-root /data/920hg_data_v4 \
  --out-dir /data/results/single/920
```
目录已有 `raw_trials.csv` 时会拒绝覆盖，避免混入旧 trial。

## 整机正式测试
```sh
$DEMO 920hg full --machine 920 \
  --data-root /data/920hg_data_v4 --cpu-list 1-95
$DEMO 920hg full --machine hg \
  --data-root /data/920hg_data_v4 --cpu-list 1-95
```
默认要求：
- CPU 列表不含 CPU 0、重复 CPU、离线 CPU。
- SMT 关闭。
- NUMA automatic balancing 关闭。
- CPU governor 为 `performance`。
- 使用 `numactl --interleave=all`。
- 每个 worker 绑定一个 CPU 并扫描完整 corpus。
- 吞吐量为全部 worker 字节数除以共享 wall-clock 时间。
`--allow-smt`、`--no-numa-interleave`、`--no-strict-env` 会改变环境，不应与默认正式基线混用。

## 构建与算法选项
- `--no-build`：不重新构建 Megascan，仍单独编译 benchmark。
- `--keep-build`：保留现有顶层 build。
- `--no-clean`：不执行 CMake `--clean-first`。
- `--expected-commit <hash>`：要求当前提交匹配。
- `--allow-dirty`：允许 tracked 修改，不建议用于正式比较。
- `--skip-hash`、`--skip-ac`、`--skip-legacy`、`--skip-accombo`、`--skip-ac-on-combo`：跳过算法。
两台机器必须使用相同构建、算法、trial 和 warmup 选项。

## Quick 检查
```sh
$DEMO 920hg quick --machine 920 \
  --data-root /data/920hg_data_v4 \
  --rule-count 500000 --hit-rate 0.5 --cpu-core 1
```
也可使用 `--hit-bps 5000`。
Quick 默认依次运行 `ac_legacy`、`ac`、`accombo`、`ac_on_combo`，并使用原 benchmark 的 block/stream 计时。它适合发现数量级异常，不替代完整矩阵。

## Profile
```sh
$DEMO 920hg profile --machine 920 \
  --data-root /data/920hg_data_v4 \
  --rule-count 500000 --hit-rate 0.5 --cpu-core 1 \
  --flamegraph-dir /opt/FlameGraph
```
Profile 使用 `RelWithDebInfo` 和 `MS_ENABLE_PROFILING=ON`。benchmark 在 timed trials 前主动停止，perf attach 后再继续，避免将加载、编译和 warmup 纳入采样。
`--perf-frequency` 可修改默认频率 `199`。每个 case 输出 `perf.data`、`perf_report.txt`、`stacks.folded` 和 `flamegraph.svg`。
Profile 依赖 `perf`、`perl` 与 FlameGraph。

## 结果汇总
已有 `raw_trials.csv` 时可重新汇总：
```sh
$DEMO 920hg summarize --out-dir /data/results/single/920
```
主要输出：
- `raw_trials.csv`：每个 trial 的原始结果。
- `summary.csv`：按机器、算法、模式、规则数和命中率汇总。
- `env.log`：commit、源码状态、CPU 配置和指纹。
- `manifest.csv`：本次数据矩阵副本。
- `run.log`：benchmark 输出。
- `block_stream_gate.csv`、`block_vs_stream.csv`：模式对比。
- `accombo_vs_ac.csv`：ACCombo/AC 对比。
- `accombo_vs_ac_on_combo.csv`、`accombo_vs_ac_same_data.csv`：同数据对比。
- `ac_vs_ac_legacy.csv`：新旧 AC 对比。
默认一个 trial，因此 p50 等于本次结果；保留 p50 字段用于兼容多 trial 数据。

## 计时标准
计时完全由 `920hg/bench_920hg.c` 实现，workflow 不另起扫描计时器，也不重新计算吞吐量。
- 使用 `CLOCK_MONOTONIC_RAW`。
- corpus/规则加载、database 编译、scratch、stream 打开、worker 创建均不计入 elapsed。
- correctness 和 warmup 不计入 elapsed。
- 完成 correctness/warmup 后才开始 timed trials。
- block 按完整 query 扫描。
- stream-full 对每条 query 执行 stream reset 和一次完整 scan。
- 整机吞吐量按全部 worker 总字节数和共享 wall-clock 计算。
- `compile_ns`、`rss_compile_growth_kb` 单独记录。
原始结果记录 expected/actual matches、expected/reported IDs 和 candidates。汇总会执行正确性校验，block/stream gate 默认允许 10% 差异。
标记为 `FAIL` 时，各 CSV 仍会完整写出，不会因为性能 gate 失败而丢失结果。

## 跨机器比较
将两台机器结果放到同一个 `WORK_ROOT`：
```text
$WORK_ROOT/results/single/920
$WORK_ROOT/results/single/hg
$WORK_ROOT/results/full/920
$WORK_ROOT/results/full/hg
```
执行：
```sh
$DEMO 920hg compare-single --work-root /data/megascan-work
$DEMO 920hg compare-full --work-root /data/megascan-work
```
也可显式指定：
```sh
$DEMO 920hg compare-single \
  --920-result /data/920/summary.csv \
  --hg-result /data/hg/summary.csv \
  --920-env /data/920/env.log \
  --hg-env /data/hg/env.log \
  --output /data/920_vs_hg.csv \
  --min-speedup 1.30
```
比较会核对 commit、source dirty、核心源码、benchmark、workflow、汇总代码和 manifest 指纹。任一指纹或 case 不一致都会标记失败。
默认最低 920/HG query_ops 比值为 `1.30`。比较文件会先完整写出，再返回 FAIL 状态。

## 公平测试要求
1. 两台机器使用同一份规则和 corpus。
2. 使用同一个 Megascan commit 和测试代码。
3. 使用相同算法、trial、warmup 和规则矩阵。
4. 单核选择可比物理核；整机使用相同物理核数量。
5. governor、SMT、NUMA 配置符合相同标准。
6. 不混用 `benchmark`、`corpus` 与 `920hg single/full` 的性能数据。
7. 保存 `raw_trials.csv`、`summary.csv`、`env.log`、`manifest.csv` 和对比文件。

## 外部依赖
920hg 流程会直接使用 `cmake`、`gcc`、`git`、`nm`、`ldd` 和 `sha256sum`。整机默认需要 `numactl`；profile 需要 `perf`、`perl` 和 FlameGraph。
demo 不调用旧测试 Shell 或 Python 脚本。

## 推荐流程
```sh
# 1. 生成一次数据，并复制同一份数据到两台机器
$DEMO 920hg generate --data-root /data/920hg_data_v4
# 2. 两台机器先运行 quick
$DEMO 920hg quick --machine 920 --data-root /data/920hg_data_v4
# 3. 正式单核和整机
$DEMO 920hg single --machine 920 --data-root /data/920hg_data_v4
$DEMO 920hg full --machine 920 --data-root /data/920hg_data_v4
# 4. HG 使用相同命令，将 machine 改为 hg
# 5. 汇集结果后比较
$DEMO 920hg compare-single
$DEMO 920hg compare-full
```

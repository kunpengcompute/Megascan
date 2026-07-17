# Megascan 920/HG 性能测试

## 数据生成逻辑

数据由固定 seed 确定性生成，并按规则数量、查询数量和命中率组成测试矩阵。HASH 使用固定长度二进制 tuple 作为规则和查询；普通 AC 使用可重复生成的 literal 规则，并在指定比例的查询 payload 中写入命中规则；ACCombo 在原子 AC 规则之上生成逻辑组合，命中查询同时包含满足组合表达式所需的原子规则。相同算法和命中率的 corpus 在不同规则规模间复用，使两台机器及不同规则规模使用相同的查询字节。

## 数据用途

- `suite_config.env`：记录整套生成参数，用于阻止不同配置写入同一数据目录。
- `manifest.csv`：列出全部测试组合及其规则文件、corpus 文件和期望参数。
- `shared/hash_*_rules.bin`：保存 HASH 使用的二进制规则。
- `shared/ac_*_rules.tsv`：保存普通 AC 使用的 literal 规则。
- `shared/accombo_*_rules.tsv`：保存 ACCombo 使用的原子规则和逻辑组合表达式。
- `shared/*corpus*.bin`：保存预生成查询、期望命中数和 stream 标识，供不同机器重复读取。
- `hash/`、`ac/`、`accombo/`：按测试组合保存 `metadata.env`，并引用对应的共享规则和 corpus。

## Demo 统一入口

构建 `megascan_demo` 后，通过一个 `920hg` 参数进入完整测试流程：

```bash
megascan_demo 920hg -h
megascan_demo 920hg generate --data-root /data/920hg_data_v4
megascan_demo 920hg single --machine 920 --data-root /data/920hg_data_v4 --cpu-core 1
megascan_demo 920hg full --machine 920 --data-root /data/920hg_data_v4 --cpu-list 1-95
megascan_demo 920hg quick --machine 920 --data-root /data/920hg_data_v4
megascan_demo 920hg profile --machine 920 --data-root /data/920hg_data_v4
megascan_demo 920hg compare-single
megascan_demo 920hg compare-full
```

`920hg` 的数据生成、矩阵调度、环境检查、CPU/NUMA 控制、结果汇总和跨机器比较均已整合到 demo 内部，不调用本目录中的 Shell 或 Python 脚本。正式测试仍单独使用 `gcc -O2 -DNDEBUG` 编译本目录的 `bench_920hg.c`，确保扫描、预热、计时和吞吐量计算继续使用原实现。每个具体操作都支持 `-h`。

每个操作使用以下目录：

- `MEGASCAN_ROOT`：Megascan 项目根目录，即包含 `ms/`、`test/` 和顶层构建文件的目录；默认根据 demo 可执行文件定位。
- `WORK_ROOT`：保存默认数据和测试结果；默认是 `${XDG_CACHE_HOME:-$HOME/.cache}/megascan/920hg`。
- `DATA_ROOT`：生成数据所在目录，内部包含 `manifest.csv`、`shared/`、`hash/`、`ac/` 和 `accombo/`；默认是 `$WORK_ROOT/data`。

可以通过环境变量指定默认目录，也可以直接使用 `--work-root`、`--data-root` 和 `--out-dir`：

```bash
MEGASCAN_ROOT=/path/to/Megascan
WORK_ROOT=/path/to/920hg-work
DATA_ROOT=/path/to/920hg-data
```

单独设置 `DATA_ROOT` 时，数据从指定目录读取，结果仍写入 `$WORK_ROOT/results`。使用 `--out-dir` 可以为本次测试指定独立结果目录。

## 文件职责

- `gen_920hg_data.c`：确定性生成 HASH、AC、ACCombo 规则和二进制 corpus。
- `bench_920hg.c`：加载预生成数据，执行 block/stream、单核/多核性能测试并输出原始指标。
- `workflow_runner.cpp`：位于上级 demo 目录，内部实现数据生成、构建、测试矩阵和 profile 调度。
- `workflow_results.cpp`：位于上级 demo 目录，内部实现 CSV 汇总与 920/HG 对比。

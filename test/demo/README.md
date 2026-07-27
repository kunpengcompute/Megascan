# Megascan Demo
`megascan_demo` 是一个参数化命令行工具：

1. 通过 `--api <接口> <参数...>` 手动调用公开接口。
2. 通过 `session` 在同一进程中手动创建、使用和释放 database、scratch、
   database/handler stream、handler、logical tree/vector，并保持 allocator 状态。
3. 执行完整 920/HG 测试，包括数据生成、单核、整机、quick、profile、汇总和跨机器比较。

接口调用与正式性能测试使用不同入口：

- `--api` 用于单接口手动测试。
- `session` 用于纯手动对象生命周期测试，demo 只保存命名句柄。
- `flow` 输出可复制、可编辑的 session 流程模板，不会偷偷执行测试。
- `920hg single`、`920hg full` 用于正式性能基线。
- `benchmark` 用于手动复现单个 case，不作为跨机器基线入口。

原有自动构造固定数据的场景、无参数单接口测试、`selftest` 和固定选项菜单均已
移除。新增的 `session` 不是预置测试菜单：每一条 API、每一个参数以及何时释放
对象都由测试人员显式输入。

## 文件说明
- `api_demo.c`：参数化 API、session 和 920hg 命令路由。
- `api_catalog_demo.c`：公开接口元数据、模块说明和统一分发入口。
- `api_cli_demo.c`：全部 standalone 公开接口的参数解析与调用。
- `api_session_demo.c`：database、scratch 和 database stream 命名状态。
- `api_session_extended_demo.c`：handler、handler stream、logical 和 allocator 状态。
- `flow_demo.c`：交付给测试人员的可复制流程模板。
- `demo_common.c`：各调用模式共用的数值、matcher 和返回码解析。
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
$DEMO benchmark -h
$DEMO generate-data -h
$DEMO session -h
$DEMO apis
$DEMO modules
$DEMO flows
$DEMO flow handler
$DEMO 920hg -h
$DEMO 920hg generate -h
$DEMO 920hg single -h
$DEMO 920hg full -h
$DEMO 920hg quick -h
$DEMO 920hg profile -h
$DEMO 920hg summarize -h
$DEMO 920hg compare-single -h
```
不带参数时只显示帮助，不会自动运行任何测试。

## 纯手动对象流程

直接进入常驻 session：

```sh
$DEMO session
```

然后逐条输入：

```text
ms_compile_database_multi db1 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 3
handles
ms_alloc_scratch db1 scratch1
ms_scan_database db1 abcdef 6 scratch1 4
ms_open_stream db1 stream1
ms_scan_stream stream1 abc 3 scratch1 4
ms_scan_stream stream1 def 3 scratch1 4
ms_flush_stream stream1 scratch1 4
ms_close_stream stream1 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
handles
exit
```

`db1`、`scratch1`、`stream1` 是当前进程内的命名句柄。前置条件采用以下统一
规则：

1. 命令提供已有句柄时，优先复用已有对象，并打印 `source=EXISTING`。
2. 没有句柄、但命令提供了足够的 fixture 参数时，demo 自动创建前置对象，
   打印 `source=AUTO reason=NO_STORED_HANDLE_FORM`，调用结束后自动清理。
3. 既没有已有句柄，也没有创建对象所需的参数时，返回具体的 `NOT_FOUND`，
   不会用无意义的默认规则构造 database。

例如下面这条使用已经存在的对象：

```text
ms_scan_database db1 abcdef 6 scratch1 4
```

输出包含：

```text
PRECONDITION api=ms_scan_database source=EXISTING database=db1 scratch=scratch1 status=READY
```

下面这条没有传句柄，但给出了完整的 database fixture，因此自动创建 database
和 scratch：

```text
ms_scan_database abcdef 6 4 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 3
```

输出包含：

```text
PRECONDITION api=ms_scan_database source=AUTO reason=NO_STORED_HANDLE_FORM status=PREPARING
PRECONDITION api=ms_scan_database source=AUTO objects=database,scratch status=READY reason=STANDALONE_CALL
```

对象归属不一致、重复创建或 database 仍有子对象时，都会在 `stage=STATE`
明确失败。

带 ext 的 database 同样由测试人员命名并暂存：

```text
ms_compile_database_multi_ext db1 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 0 0 0 0 0 0 3 6 20 3
```

注意：session 每行是一条完整命令，目前不把 shell 的反斜杠续行解释为同一条
命令。交互输入时请将单条 API 写在一行；较长流程建议使用 command file。

也可以把同样的命令写入文件执行：

```sh
$DEMO session api-flow.txt
echo $?
```

脚本模式只要有一条命令失败，最终退出码就是非 0。若退出时仍有未释放句柄，
demo 会打印 `SESSION_CLEANUP`，安全回收能够回收的对象，并以
`SESSION_FAILED reason=UNRELEASED_HANDLES` 判定流程失败。因此正常流程必须由
测试人员亲自 close/free，不能依赖退出清理。

`MS_FIND_FAIL` 会显示为 `NO_MATCH`，不是含义模糊的成功；真正的库错误显示
`ERROR stage=API`，CLI 数量或类型错误显示 `ERROR stage=CLI`，对象状态错误显示
`ERROR stage=STATE`。

## 可复制的测试流程

查看当前流程：

```sh
$DEMO flows
```

当前提供：

```text
block
stream
handler
hash-handler
delete-handler-ac-rule
delete-handler-ac-combination
logical
allocator
delete
delete-database-ac-rule
delete-database-ac-combination
owner-error
```

查看一个流程不会调用任何接口，只会输出 session 脚本：

```sh
$DEMO flow handler
```

建议测试人员先保存、检查和修改参数，再执行：

```sh
$DEMO flow handler > handler.flow
cat handler.flow
$DEMO session handler.flow
echo $?
```

正常流程也可以直接管道执行：

```sh
$DEMO flow block | $DEMO session
$DEMO flow stream | $DEMO session
$DEMO flow handler | $DEMO session
$DEMO flow hash-handler | $DEMO session
$DEMO flow delete-handler-ac-rule | $DEMO session
$DEMO flow delete-handler-ac-combination | $DEMO session
$DEMO flow delete-database-ac-rule | $DEMO session
$DEMO flow delete-database-ac-combination | $DEMO session
$DEMO flow logical | $DEMO session
$DEMO flow allocator | $DEMO session
```

`delete` 和 `owner-error` 包含预期失败步骤，session 最终退出码应为非 0，必须结合
脚本开头的 `# Expected` 和具体错误输出判断是否通过。

Handler 流程中的名称同样由测试人员指定：

```text
ms_handler_init handler1 ac 0 0
ms_compile_multi handler1 abc def '101&102' 2 2 1 101 102 1001 3
ms_scan_expr handler1 abcdef 6 8
ms_open_handler_stream handler1 hstream1
ms_scan_expr_stream hstream1 abc 3 8
ms_scan_expr_stream hstream1 def 3 8
ms_close_handler_stream hstream1
ms_handler_destroy handler1
```

Logical 流程使用 fixture 命令创建 vector，以及创建可供
`ms_logical_eval_inplace` 直接计算的操作树；其余均为公开 API。
`ms_logical_parse` 创建的树包含引用节点，用于 `ms_logical_eval`；
`logical_tree_create_op` 则直接构造一个操作节点，用于单独、公平地调用
`ms_logical_eval_inplace`：

```text
ms_logical_parse tree1 '101&!102'
logical_vector_create vector1 256
ms_logical_set vector1 101 1
ms_logical_set vector1 102 0
ms_logical_eval tree1 vector1
logical_tree_create_op tree2 200 or 101 102
ms_logical_eval_inplace tree2 vector1
logical_vector_free vector1
ms_logical_tree_destroy tree1
ms_logical_tree_destroy tree2
```

Allocator 在 session 内保持到测试人员恢复默认值：

```text
allocator_stats reset
ms_set_database_allocator custom custom custom
ms_compile_database_multi db1 ac 0 0 abc 0 101 1
ms_free_database db1
allocator_stats
ms_set_database_allocator default default default
allocator_stats
```

如果退出前没有恢复 allocator，session 会恢复默认值，但同时打印
`SESSION_CLEANUP type=allocator`，并将流程判定为失败。

## 接口调用示例
查看接口、参数格式和模块关系：

```sh
$DEMO apis
$DEMO --api ms_scan_database -h
$DEMO --api ms_set_scratch_allocator -h
$DEMO modules
$DEMO --module stream
```

`--api` 后必须提供接口参数；缺少参数时返回非 0，不会自动创建固定规则进行
测试。输出采用紧凑格式，例如：

```text
ms_scan_database ret=0(MS_SUCCESS)
count=1 ids=1001
```

必要的前置对象由 CLI 根据 fixture 参数创建，并用一条紧凑的 `PRECONDITION`
记录说明来源、对象和结果；调用结束后用 `CLEANUP` 或具体释放结果说明清理状态。

这里需要区分输入对象和输出对象：

- `ms_compile_database_multi(ms_database_t **db, ...)` 中的 `db` 是输出参数。
  调用前由 CLI 创建一个值为 NULL 的 `ms_database_t *db`，并把 `&db` 传给接口；
  它不需要一个已经存在的 database。
- `ms_scan_database(const ms_database_t *db, ...)` 中的 `db` 是输入对象。
  单次 shell 命令不能复用另一个进程里的 C 指针，因此 CLI 使用命令末尾的
  database fixture 参数创建它，并在输出中明确显示
  `source=AUTO objects=database,scratch status=READY`。
- 规则、flags、ID、长度、容量等业务参数全部由测试人员手动提供。进程内对象
  地址不能手工填写；接受任意十六进制地址反而会让测试产生悬空指针和崩溃，
  不属于公平的接口参数测试。

编译类接口的成功结果现在包含三部分：

```text
ms_compile_database_multi ret=0(MS_SUCCESS)
output.db=CREATED
cleanup.ms_free_database ret=0(MS_SUCCESS)
```

三项分别证明目标接口返回成功、确实产生了 database、demo 最终释放成功。
如果只看到 `ret=0` 而 `output.db=NULL`，demo 会返回非 0，测试不算通过。

扫描类接口还会显示 fixture 状态和实际匹配结果：

```text
PRECONDITION api=ms_scan_database source=AUTO objects=database,scratch status=READY reason=STANDALONE_CALL
ms_scan_database ret=0(MS_SUCCESS)
count=1 ids=1001
```

如果 fixture 构建失败，会输出 `ms_scan_database called=NO`，不会把前置步骤的
失败伪装成目标接口返回值。

如果需要按 CLI 参数调用接口，在接口名后继续传入参数：

```sh
$DEMO --api ms_create_database ac 0 0
$DEMO --api ms_compile_database_multi \
  ac 0 0 \
  abc def '101&102' \
  2 2 1 \
  101 102 1001 \
  3
$DEMO --api ms_compile_database_multi_ext ac 0 0 \
  abc def '101&102' \
  2 2 1 \
  101 102 1001 \
  0 0 0  0 0 0  3 6 20 \
  3
$DEMO --api ms_alloc_scratch ac 0 0 abc 0 101 1
$DEMO --api ms_scan_database abcdef 6 4 \
  ac 0 0 abc def '101&102' 2 2 1 101 102 1001 3
$DEMO --api ms_scan_stream abc 3 4 ac 0 0 abc 0 101 1
$DEMO --api ms_flush_stream 4 ac 0 0 abc '!101' 2 1 101 2001 2
$DEMO --api ms_remove_database_rule \
  101 3 abc 1001 0 - -- 2 abcdef 4 \
  ac 0 0 abc def '101&102' 0 0 1 101 102 1001 3
$DEMO --api ms_find_rule 0 7 blocked hash 16 4 7001 7 blocked
$DEMO --api ms_scan_expr abcdef 6 4 \
  abc def '101&102' 2 2 1 101 102 1001 3
$DEMO --api ms_compile_eval_combinations 101,102 2 4 \
  abc def '101&102' 2 2 1 101 102 1001 3
$DEMO --api ms_logical_eval '101&!102' 101=1,102=0 256
$DEMO --api ms_set_database_allocator custom custom custom
```

当前注册的 43 个公开接口均只提供参数化调用。映射规则是：

- 对象指针、输出数组和输出计数由 CLI 托管，不接受真实地址。
- 其余调用方参数按照头文件中的先后顺序排列。
- `expressions`、`flags`、`ids` 是三个独立数组，不再交叉为规则三元组。
- `ext` 按 `flags min_offset max_offset` 连续表示每个 `ms_expr_ext_t`。
- `len`、`elements`、`rule_num`、`alias_count` 必须显式传入，不再自动推导。
- 规则结构按 `alias key_len key` 表示；NULL key 使用 `-`。
- alias 集合：逗号分隔，例如 `101,102`。
- logical 状态：逗号分隔，例如 `101=1,102=0`。
- allocator 的 alloc/calloc/free 分别传 `custom` 或 `default`。

CLI 能安全表达的参数与 C 参数一一对应；真实地址类参数例外：

- database、scratch、stream、handler 等对象指针由 fixture 创建。
- `ids`、`id_count`、`matched` 等输出指针由 CLI 分配。
- CLI 不接受任意数值伪造为地址，因此 NULL、悬空指针和错误对象归属测试仍应由
  C 单元测试完成，不能通过 shell 公平、安全地表达。
- 数组序列化项数必须严格等于 `elements`。例如序列化 3 条规则却传
  `elements=2` 会在调用库之前返回 `COUNT_MISMATCH`，退出码非 0。

例如：

```c
ms_compile_database_multi_ext(&db, type, buckets, secondary,
                              expressions, flags, ids, ext, elements);
```

对应：

```text
<type> <buckets> <secondary>
<expressions[0..elements)>
<flags[0..elements)>
<ids[0..elements)>
<ext[0..elements)，每项 flags/min/max>
<elements>
```

`db` 是输出指针，由 CLI 内部提供并在调用结束后释放。

`flags` 是位掩码：`1=MS_FLAG_COMBINATION`、`2=MS_FLAG_QUIET`、
`4=MS_FLAG_SINGLEMATCH`，可以按位组合，例如
`5=COMBINATION|SINGLEMATCH`。`SINGLEMATCH` 只允许用于组合规则。
`ext_flags` 同样是位掩码：`0=无约束`、`1=MIN_OFFSET`、
`2=MAX_OFFSET`、`3=MIN|MAX`。

生命周期类接口同样要求参数。CLI 会根据参数创建目标接口所需的最小前置对象，
调用目标接口后执行必要清理。例如 `ms_free_database` 先创建 database；
`ms_close_stream` 先编译 database、分配 scratch 并打开 stream。

`ms_scan_stream` 的一次 CLI 调用只执行一次目标函数，并显式接受 `data` 和
`len`。它不再通过 `|` 偷偷执行多次扫描。跨 chunk 场景属于多接口/多调用流程，
不能冒充一次单接口调用。

按模块查看全部格式：

```sh
$DEMO --api -h
$DEMO --api ms_create_database -h
$DEMO --api ms_scan_stream -h
$DEMO --api ms_compile_multi_ext -h
$DEMO --api ms_scan_expr_stream -h
$DEMO --api ms_logical_eval_inplace -h
$DEMO --api ms_set_stream_allocator -h
```

## 与 gnsj 功能设计对齐

`log/Megascan/gnsj` 下五份功能设计是本 demo 的功能契约。接口测试按以下
关系组织。这里的“验证”表示测试人员可通过对应参数构造该行为，不再表示 demo
内部存在一套写死的自动回归：

| 功能设计 | 主要接口 | demo 必须验证的行为 |
| --- | --- | --- |
| 对外接口适配 | `ms_compile_database_multi[_ext]`、`ms_alloc_scratch`、`ms_scan_database`、stream 生命周期 | 手动指定 database、scratch、block/stream 参数和 MIN/MAX offset |
| AC 规则逻辑组合 | 编译、Block/Stream 扫描、logical API | `QUIET` 不直接上报、组合 ID 上报、跨 chunk 组合、纯否定 flush、offset；flags 按位支持 `SINGLEMATCH` |
| AC 流模式 | `ms_open/scan/flush/close/reset_stream` | 每个 `--api` 只调用一次目标接口；data、len 和 capacity 与签名对应 |
| 自定义内存 | 五个 `ms_set_*allocator` 接口 | custom 模式不只调用 setter，还创建并释放对应 database/scratch/stream，检查回调计数；misc 明确标注暂无公开消费者 |
| 动态删除 | `ms_remove_rule`、`ms_remove_database_rule` | 分别验证 HASH handler 按 key、AC handler 普通规则按 alias+key、AC handler 组合规则按 alias、AC database 普通/组合规则删除，以及 database 原子失败、scratch 刷新、stream stale/reset |

参数化入口遵循功能边界：HASH database 不支持 stream 和
`ms_remove_database_rule`，HASH 动态删除使用 handler 的 `ms_remove_rule`。

## 模块接口流程

模块命令展示调用顺序和所属接口：

```sh
$DEMO modules
$DEMO --module database
$DEMO --module block
$DEMO --module stream
$DEMO --module delete
$DEMO --module allocator
```

模块命令只展示接口关系，不自动执行接口。

## AC Database 动态删除端到端测试

动态删除的定位语义必须先区分对象和规则类型，不能笼统写成“删除一个 ID”：

| 用例 | 对象 | 实际接口 | 删除对象 | 定位字段 |
| --- | --- | --- | --- | --- |
| `hash-handler` | `ms_handler_t` HASH | `ms_remove_rule` | HASH 普通规则 | `key + key_len`；alias 不参与定位 |
| `delete-handler-ac-rule` | `ms_handler_t` AC | `ms_remove_rule` | AC 普通规则 | `alias + key + key_len` |
| `delete-handler-ac-combination` | `ms_handler_t` AC | `ms_remove_rule` | AC 组合规则 | `alias`；`key=NULL`、`key_len=0` |
| `delete-database-ac-rule` / `delete` | `ms_database_t` AC | `ms_remove_database_rule` | AC 普通规则 | `alias + key + key_len` |
| `delete-database-ac-combination` | `ms_database_t` ACCombo | `ms_remove_database_rule` | AC 组合规则 | `alias`；`key=NULL`、`key_len=0` |

上述 flow 是测试人员可编辑的手动功能流程；920HG 端到端矩阵只测试当前正式
database 路径，不把 handler 兼容接口混入 database 删除延迟统计。

动态删除不是独立的玩具规则子命令，而是 `single`、`full` 和 `quick` 矩阵中默认启用的正式测试项：

```sh
$DEMO 920hg single --machine 920 --data-root /data/920hg_data_v4 --cpu-core 1
$DEMO 920hg full --machine 920 --data-root /data/920hg_data_v4 --cpu-list 1-95
$DEMO 920hg quick --machine 920 --data-root /data/920hg_data_v4
```

矩阵会新增三类动态删除用例：

- `ac_delete`：从普通 AC database 按 `alias+key` 删除普通规则。
- `accombo_delete`：从 ACCombo database 按 `alias+key` 删除普通依赖规则，
  验证依赖它的组合规则随之消失。
- `accombo_combination_delete`：从 ACCombo database 以 `key=NULL`、
  `key_len=0` 按 alias 直接删除组合规则，普通规则继续存在。

它们读取
manifest 中的真实规则文件和完整 corpus，在 worker 全部暂停后执行失败批次
原子性检查和成功删除，然后验证旧 scratch 自动刷新、所有旧 stream 返回
`MS_STREAM_STALE`、reset 后按新规则完成 Block/Stream 全量扫描。

动态删除不再写成普通扫描吞吐量行。每个 `--trials` 轮次都会重新编译一份
未删除的 database，只计时一次成功的 `ms_remove_database_rule()`；该时间包含
接口内部导出剩余规则、重建 AC database 和提交新版本的完整耗时。原始结果写入
`raw_delete_trials.csv`，汇总写入 `dynamic_delete_summary.csv`，包括
`delete_ns_min/p50/p95/max` 和单规则 `rules_per_second_p50`。普通
`raw_trials.csv`、`summary.csv` 及 Block/Stream 吞吐量口径保持不变。
原始删除 CSV 还记录 `target_kind`、`locator`、`removed_alias`、
`removed_key_len`、`removed_key` 和 `affected_id`，因此可以直接确认每轮究竟
删除的是普通规则还是组合规则、按什么字段定位，而不是只看到一个模糊的 ID。
这些目标字段同样保留在 `dynamic_delete_summary.csv` 中；汇总结果不会丢失
每组 trial 实际删除的 alias 和 key。
每轮还会把以下行为拆成独立结果列，而不是合并成一个 `correctness=PASS`：

- `failed_batch_rejected`：包含不存在规则的批次是否返回失败。
- `failed_batch_unchanged`：失败批次后 live database 是否保持不变。
- `delete_success`：目标规则是否删除成功。
- `repeat_delete_rejected`：再次删除同一规则是否返回不存在。
- `scratch_refresh`：旧 scratch 是否自动切换到新 generation。
- `stream_stale`：旧 stream 是否返回 `MS_STREAM_STALE`。
- `stream_reset`：旧 stream 是否能够 reset。
- `post_delete_block`、`post_delete_stream`：删除后的完整 corpus 是否符合预期。
- `semantic_effect`：普通规则、依赖规则级联或组合规则直接删除的语义是否正确。

运行日志使用 `DELETE_CHECK name=... expected=... actual=... status=PASS|FAIL`
逐项输出；`raw_delete_trials.csv` 保存每轮分项结果，
`dynamic_delete_summary.csv` 保存跨 trial 汇总后的分项结果。
整机模式下仍只由主线程调用一次删除 API；多个 worker 只用于验证各自持有的
旧 scratch/stream 在 database 版本变化后表现一致，删除速率不是多核聚合值。
需要临时跳过时传入 `--skip-dynamic-delete`。

## Allocator 定点测试

每类 allocator 可以单独测试：

```sh
$DEMO --api ms_set_allocator custom custom custom
$DEMO --api ms_set_database_allocator custom custom custom
$DEMO --api ms_set_scratch_allocator custom custom custom
$DEMO --api ms_set_stream_allocator custom custom custom
$DEMO --api ms_set_misc_allocator custom custom custom
```

Database、scratch 和 stream 测试会实际创建并释放对应对象，然后检查自定义回调计数。当前没有使用 misc allocator 的公开对象创建接口，因此 misc 测试只验证 setter 和恢复默认值，并在输出中明确标注该边界。
allocator 接口按函数签名依次传入 alloc、calloc 和 free 三个选择。
逻辑求值：
```sh
$DEMO --api ms_logical_eval '101&!102' 101=1,102=0 256
```

## 手动 Benchmark
设置已有数据目录：
```sh
DATA=/home/data/Megascan/920hg_data_v4
```
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
- `raw_delete_trials.csv`：每轮使用新 database 测得的动态删除原始延迟，并记录删除对象类型、定位方式、alias 和 key。
- `summary.csv`：按机器、算法、模式、规则数和命中率汇总。
- `dynamic_delete_summary.csv`：动态删除延迟的 min、p50、p95、max、单规则速率和各个正确性分项。
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
- 动态删除不计入扫描吞吐量；每轮重新构建初始 database，仅计时
  `ms_remove_database_rule()`，结果进入独立删除 CSV。
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
6. 不混用 `benchmark` 与 `920hg single/full` 的性能数据。
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

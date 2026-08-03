# Megascan Demo 测试用例与运行命令

本文档是 `megascan_demo` 的功能测试清单，文档中的“预期失败”表示命令应返回非 0；这是测试通过的预期结果，不是测试脚本自身异常。

## 1. 测试准备

```sh
cd /path/to/Megascan/test
python3 build.py megascan_demo debug

DEMO=./build/bin/megascan_demo
$DEMO -h
$DEMO apis
```

查看单接口参数：

```sh
$DEMO --api ms_compile_database_multi_ext -h
$DEMO --api ms_scan_database -h
$DEMO --api ms_remove_database_rule -h
$DEMO session -h
```

每条命令执行后均应检查退出码：

```sh
echo $?
```

退出码约定：

| 退出码 | 含义 |
| --- | --- |
| `0` | API 成功，或接口正常返回 `MS_FIND_FAIL/NO_MATCH` |
| 非 `0` | CLI 参数错误、状态错误、API 错误、清理错误或 session 有预期/非预期失败 |

输出字段约定：

| 输出 | 含义 |
| --- | --- |
| `source=EXISTING` | 使用 session 中已经存在的对象 |
| `source=AUTO` | demo 根据 fixture 参数自动创建前置对象 |
| `OK` | API 返回 `MS_SUCCESS` |
| `NO_MATCH` | API 返回 `MS_FIND_FAIL`，接口调用正常但没有上报 ID |
| `ERROR stage=CLI` | 参数格式、数量或范围错误，目标 API 通常未调用 |
| `ERROR stage=STATE` | 句柄不存在、重复或归属错误 |
| `ERROR stage=API` | 目标 Megascan API 返回错误 |
| `called=NO` | 失败发生在前置条件或参数阶段，目标 API 没有调用 |
| `CLEANUP ... MS_SUCCESS` | 自动创建的对象已成功释放 |

## 2. 公共参数说明

测试人员可以先查看或导出可执行流程模板：

```sh
$DEMO flows
$DEMO flow block
$DEMO flow handler > handler.flow
$DEMO session handler.flow
```

`flow` 只输出命令和预期检查点；是否执行、使用什么参数仍由测试人员决定。

表达式数组必须按 C 接口顺序传递：

```text
expressions[N] flags[N] ids[N] elements
```

带 ext 时：

```text
expressions[N] flags[N] ids[N]
ext[0].flags ext[0].min_offset ext[0].max_offset
...
ext[N-1].flags ext[N-1].min_offset ext[N-1].max_offset
elements
```

数量必须严格满足：

```text
expression数量 == flags数量 == ids数量 == ext数量 == elements
```

flags：

| 值 | 含义 |
| --- | --- |
| `1` | `MS_FLAG_COMBINATION` |
| `2` | `MS_FLAG_QUIET` |
| `4` | `MS_FLAG_SINGLEMATCH` |
| `5` | `COMBINATION | SINGLEMATCH` |

ext flags：

| 值 | 含义 |
| --- | --- |
| `0` | 无 offset 约束 |
| `1` | `MIN_OFFSET` |
| `2` | `MAX_OFFSET` |
| `3` | `MIN_OFFSET | MAX_OFFSET` |

## 3. Database、Scratch 与 Block

### TC-31-001：编译 AC database

类型：正常用例。

```sh
$DEMO --api ms_compile_database_multi \
  ac 0 0 \
  abc def \
  0 0 \
  101 102 \
  2
```

预期退出码：`0`。

关键输出：

```text
ms_compile_database_multi ret=0(MS_SUCCESS)
output.db=CREATED
cleanup.ms_free_database ret=0(MS_SUCCESS)
```

### TC-31-002：编译带 ext 的 AC database

类型：正常用例。

```sh
$DEMO --api ms_compile_database_multi_ext \
  ac 0 0 \
  abc def '101&102' \
  2 2 1 \
  101 102 1001 \
  0 0 0 \
  0 0 0 \
  3 6 20 \
  3
```

预期退出码：`0`，`output.db=CREATED`。

### TC-31-003：手动创建并复用 scratch

类型：正常生命周期用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc 0 101 1
ms_alloc_scratch db1 scratch1
handles
ms_scan_database db1 abc 3 scratch1 4
ms_scan_database db1 xxx 3 scratch1 4
ms_scan_database db1 abc 3 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
handles
exit
EOF
```

预期退出码：`0`。

关键结果：

```text
第一次扫描 ids=101
第二次扫描 NO_MATCH
第三次扫描 ids=101
SESSION_RESULT commands_failed=0
```

该用例同时验证 Block 扫描每次从空状态开始，scratch 可以复用。

### TC-31-004：无已有对象时自动创建前置条件

类型：正常自动 fixture 用例。

```sh
$DEMO --api ms_scan_database \
  abc 3 4 \
  ac 0 0 \
  abc \
  0 \
  101 \
  1
```

预期退出码：`0`。

关键输出：

```text
PRECONDITION ... source=AUTO objects=database,scratch status=READY
count=1 ids=101
CLEANUP source=AUTO ret=0(MS_SUCCESS)
```

### TC-31-E001：数组数量与 elements 不一致

类型：预期失败，CLI 参数校验。

```sh
$DEMO --api ms_compile_database_multi_ext \
  ac 0 0 \
  abc def '101&102' \
  2 2 1 \
  101 102 1001 \
  0 0 0 \
  0 0 0 \
  3 6 20 \
  2
```

预期退出码：非 `0`。

关键输出：

```text
序列化了 3 条 expression/flag/id/ext，但 elements=2
ms_compile_database_multi_ext called=NO
```

### TC-31-E002：Block 输出容量不足

类型：预期失败，API 错误。

```sh
$DEMO --api ms_scan_database \
  abcdef 6 1 \
  ac 0 0 \
  abc def \
  0 0 \
  101 102 \
  2
```

预期退出码：非 `0`。

预期 API 返回：`MS_INVALID_PARA`，实际需要容量为 `2`。

### TC-31-E003：scratch 与 database 归属不一致

类型：预期失败，session 状态校验。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc 0 101 1
ms_compile_database_multi db2 ac 0 0 def 0 102 1
ms_alloc_scratch db1 scratch1
ms_scan_database db2 def 3 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
ms_free_database db2
exit
EOF
```

预期退出码：非 `0`。

关键输出：

```text
ERROR stage=STATE ... reason=OWNER_MISMATCH actual_db=db1
```

注意：该命令验证 demo 状态管理；API 自身的错误 scratch 校验由 C 单元测试覆盖。

### TC-31-E004：database 存在子对象时提前释放

类型：预期失败，生命周期校验。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc 0 101 1
ms_alloc_scratch db1 scratch1
ms_free_database db1
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期退出码：非 `0`。

关键输出：

```text
ERROR stage=STATE api=ms_free_database reason=LIVE_SCRATCH_OR_STREAM
```

## 4. AC 逻辑组合

### TC-32-001：QUIET 普通规则参与 AND 组合

类型：正常用例。

```sh
$DEMO --api ms_scan_database \
  abcdef 6 4 \
  ac 0 0 \
  abc def '101&102' \
  2 2 1 \
  101 102 1001 \
  3
```

预期退出码：`0`。

预期：

```text
count=1 ids=1001
```

结果中不应出现 `101`、`102`。

### TC-32-002：OR 组合

类型：正常用例。

```sh
$DEMO --api ms_scan_database \
  abc 3 4 \
  ac 0 0 \
  abc def '101|102' \
  2 2 1 \
  101 102 1001 \
  3
```

预期：`ids=1001`。

### TC-32-003：NOT 组合

类型：正常用例。

```sh
$DEMO --api ms_scan_database \
  abc 3 4 \
  ac 0 0 \
  abc def '101&!102' \
  2 2 1 \
  101 102 1001 \
  3
```

预期：`ids=1001`。

### TC-32-004：逻辑 API 求值

类型：正常用例。

```sh
$DEMO --api ms_logical_eval '101&!102' 101=1,102=0 256
```

预期：

```text
ret=0(MS_SUCCESS)
matched=1
```

### TC-32-005：组合规则满足 offset 范围

类型：正常用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi_ext db1 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 0 0 0 0 0 0 3 6 20 3
ms_alloc_scratch db1 sc1
ms_scan_database db1 abcdef 6 sc1 16
ms_free_scratch sc1
ms_free_database db1
exit
EOF
```

预期：

- `abcdef` 中规则 `101`、`102` 均成立；
- 组合规则 `1001` 的结束位置满足 `[6, 20]`；
- 命中明细包含 `id=1001`。

### TC-32-006：组合规则不满足 offset 范围

类型：正常用例，验证不命中分支。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi_ext db1 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 0 0 0 0 0 0 3 0 5 3
ms_alloc_scratch db1 sc1
ms_scan_database db1 abcdef 6 sc1 16
ms_free_scratch sc1
ms_free_database db1
exit
EOF
```

预期：

- 普通规则仍可命中；
- 命中明细中不应出现 `id=1001`，因为其结束位置超出 `[0, 5]`。

### TC-32-007：持久 handler 的 Block 与跨 chunk Stream

```sh
$DEMO flow handler > handler.flow
$DEMO session handler.flow
```

预期：

- `ms_handler_init`、`ms_compile_multi` 和 `ms_scan_expr` 成功；
- Block 命中 `id=1001`；
- 第一个 stream chunk 返回 `NO_MATCH`；
- 第二个 chunk 命中 `id=1001`；
- handler stream 和 handler 均由测试人员显式释放；
- `SESSION_RESULT commands_failed=0`。

### TC-32-008：持久 logical tree/vector

```sh
$DEMO flow logical > logical.flow
$DEMO session logical.flow
```

预期：

- tree 和 vector 创建成功；
- `ms_logical_get` 返回 `value=1`；
- `ms_logical_eval` 返回 `matched=1`；
- `logical_tree_create_op` 创建直接操作树成功；
- `ms_logical_eval_inplace` 返回 `matched=1`；
- tree/vector 均显式释放；
- `SESSION_RESULT commands_failed=0`。

### TC-32-E001：重复规则 ID

类型：预期失败，API 编译检查。

```sh
$DEMO --api ms_compile_database_multi \
  ac 0 0 \
  abc def \
  0 0 \
  101 101 \
  2
```

预期退出码：非 `0`，返回 `MS_INVALID_PARA`。

### TC-32-E002：组合引用不存在的普通规则

类型：预期失败。

```sh
$DEMO --api ms_compile_database_multi \
  ac 0 0 \
  abc '101&999' \
  2 1 \
  101 1001 \
  2
```

预期：`MS_INVALID_PARA`。

### TC-32-E003：普通规则使用 SINGLEMATCH

类型：预期失败。

```sh
$DEMO --api ms_compile_database_multi ac 0 0 abc 4 101 1
```

预期：`MS_INVALID_PARA`。

### TC-32-E004：HASH database 编译组合规则

类型：预期失败。

```sh
$DEMO --api ms_compile_database_multi \
  hash 16 4 \
  abc '101' \
  0 1 \
  101 1001 \
  2
```

预期：`MS_NOT_SUPPORTED`。

### TC-32-E005：非法逻辑表达式

类型：预期失败。

分别执行：

```sh
$DEMO --api ms_logical_parse '101&&102'
$DEMO --api ms_logical_parse '(101|102'
$DEMO --api ms_logical_parse '101+102'
```

每条命令预期退出码均为非 `0`，返回 `MS_INVALID_PARA`。

### TC-32-E006：ext 最小偏移大于最大偏移

类型：预期失败。

```sh
$DEMO --api ms_compile_database_multi_ext \
  ac 0 0 \
  abc '101' \
  2 1 \
  101 1001 \
  0 0 0 \
  3 20 10 \
  2
```

预期：`MS_INVALID_PARA`。

## 5. AC Stream

### TC-33-001：固定字符串跨 chunk

类型：正常用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abcdef 0 101 1
ms_alloc_scratch db1 scratch1
ms_open_stream db1 stream1
ms_scan_stream stream1 abc 3 scratch1 4
ms_scan_stream stream1 def 3 scratch1 4
ms_close_stream stream1 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期退出码：`0`。

预期：

```text
第一个 chunk：NO_MATCH
第二个 chunk：ids=101
```

### TC-33-002：组合规则跨 chunk

类型：正常用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 3
ms_alloc_scratch db1 scratch1
ms_open_stream db1 stream1
ms_scan_stream stream1 abc 3 scratch1 4
ms_scan_stream stream1 def 3 scratch1 4
ms_close_stream stream1 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期：

```text
abc：NO_MATCH
def：ids=1001
SESSION_RESULT commands_failed=0
```

### TC-33-003：reset 清空跨 chunk 状态

类型：正常用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 3
ms_alloc_scratch db1 scratch1
ms_open_stream db1 stream1
ms_scan_stream stream1 abc 3 scratch1 4
ms_reset_stream stream1
ms_scan_stream stream1 def 3 scratch1 4
ms_close_stream stream1 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期：reset 后扫描 `def` 不应上报 `1001`。

### TC-33-004：flush 计算纯否定规则

类型：正常用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc '!101' 2 1 101 1001 2
ms_alloc_scratch db1 scratch1
ms_open_stream db1 stream1
ms_flush_stream stream1 scratch1 4
ms_close_stream stream1 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期：flush 上报 `1001`。

### TC-33-E001：HASH database 不支持 stream

类型：预期失败，API 错误。

```sh
$DEMO session <<'EOF'
ms_create_database db1 hash 16 4
ms_open_stream db1 stream1
ms_free_database db1
exit
EOF
```

预期退出码：非 `0`。

关键输出：`MS_NOT_SUPPORTED`，`output.stream=NULL`。

### TC-33-E002：stream 输出容量不足后重试

类型：包含一次预期失败的回滚用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc 0 101 1
ms_alloc_scratch db1 scratch1
ms_open_stream db1 stream1
ms_scan_stream stream1 abc 3 scratch1 0
ms_scan_stream stream1 abc 3 scratch1 4
ms_close_stream stream1 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期整个 session 退出码：非 `0`，因为第一条 scan 是预期失败。

关键检查：

```text
第一次：MS_INVALID_PARA，required_capacity=1
第二次：ids=101
```

第二次能够命中证明第一次失败没有提交 stream 状态。

### TC-33-E003：stream 使用错误 database 的 scratch

类型：预期失败，session 状态校验。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc 0 101 1
ms_compile_database_multi db2 ac 0 0 def 0 102 1
ms_alloc_scratch db2 scratch2
ms_open_stream db1 stream1
ms_scan_stream stream1 abc 3 scratch2 4
ms_close_stream stream1 scratch2 4
exit
EOF
```

预期：`OWNER_MISMATCH`。由于 close 同样使用错误 scratch，session 退出清理也会报告未释放对象。

API 层错误 scratch 由 `test_database_stream_rejects_wrong_scratch` 覆盖。

## 6. 自定义内存

### TC-34-001：设置全部 allocator

```sh
$DEMO --api ms_set_allocator custom custom custom
```

预期：

```text
ret=0(MS_SUCCESS)
route=PASS
malloc/calloc/free 计数非零
```

### TC-34-002：database allocator

```sh
$DEMO --api ms_set_database_allocator custom custom custom
```

预期：`route=PASS`，database 类回调计数非零。

### TC-34-003：scratch allocator

```sh
$DEMO --api ms_set_scratch_allocator custom custom custom
```

预期：`route=PASS`，scratch calloc/free 计数非零。

### TC-34-004：stream allocator

```sh
$DEMO --api ms_set_stream_allocator custom custom custom
```

预期：`route=PASS`，stream calloc/free 计数非零。

### TC-34-005：misc allocator

```sh
$DEMO --api ms_set_misc_allocator custom custom custom
```

预期：

```text
ret=0(MS_SUCCESS)
route=N/A(misc_has_no_public_consumer)
```

### TC-34-006：单独恢复某类回调为默认值

```sh
$DEMO --api ms_set_database_allocator custom default custom
```

预期：setter 返回成功；命令结束后 demo 将 allocator 全部恢复为默认状态。

### TC-34-007：session 中手动设置、使用并恢复 allocator

```sh
$DEMO flow allocator > allocator.flow
$DEMO session allocator.flow
```

预期：

- database 创建和释放均成功；
- `allocator_stats` 中 malloc/calloc/free 计数增加；
- 最后一次 `allocator_stats` 显示 `active_mask=0`；
- `SESSION_RESULT commands_failed=0`。

### TC-34-008：对象存活期间顺序切换 allocator

```sh
$DEMO session <<'EOF'
allocator_stats reset
ms_set_database_allocator custom custom custom
ms_compile_database_multi db1 ac 0 0 abc 0 101 1
ms_set_database_allocator default default default
ms_free_database db1
allocator_stats
exit
EOF
```

预期：database释放成功，自定义`free`计数增加，`active_mask=0`，
`SESSION_RESULT commands_failed=0`。这证明已有内存使用申请时记录的free，
而不是释放时的当前全局配置。

### TC-34-E001：分配失败和中间资源清理

当前 demo 的 allocator 参数只支持 `custom/default`，不能指定第 N 次申请失败。
该用例必须运行 C 单元测试：

```sh
cd /path/to/Megascan/test
python3 build.py testdemo_ut debug
./build/bin/testdemo_ut
```

重点检查：

- `test_allocator_hash_init_failure_frees_created_table`
- `test_allocator_hash_database_init_failure_frees_created_table`
- `test_allocator_database_scan_reuses_scratch_workspace`
- `test_allocator_database_stream_scan_reuses_scratch`
- `test_allocator_objects_keep_original_free_after_reconfigure`

## 7. 动态删除

### TC-35-001：删除普通规则并移除依赖组合

类型：正常用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 3
ms_alloc_scratch db1 scratch1
ms_scan_database db1 abcdef 6 scratch1 4
ms_remove_database_rule db1 101 3 abc 1
ms_scan_database db1 abcdef 6 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期：

```text
TARGET api=ms_remove_database_rule object=database name=db1 index=0 alias=101 key_len=3 key=abc locator=alias+key
删除前：ids=1001
删除后：NO_MATCH
```

删除 `101` 后，依赖它的组合规则 `1001` 同时删除。

### TC-35-002：只删除组合规则

类型：正常用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc def '101&102' 0 0 1 101 102 1001 3
ms_alloc_scratch db1 scratch1
ms_scan_database db1 abcdef 6 scratch1 4
ms_remove_database_rule db1 1001 0 - 1
ms_scan_database db1 abcdef 6 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期：

```text
TARGET api=ms_remove_database_rule object=database name=db1 index=0 alias=1001 key_len=0 key= locator=alias
删除前包含：101,102,1001
删除后只包含：101,102
```

### TC-35-003：HASH handler 按 key 删除

类型：正常兼容接口用例。

```sh
$DEMO flow hash-handler > hash-handler.flow
cat hash-handler.flow
$DEMO session hash-handler.flow
```

该流程添加 `alias=7001, key=blocked`，删除时故意传入不同的
`alias=9999`，但使用同一个 key。预期：

```text
第一次 ms_find_rule：OK，alias=7001
TARGET api=ms_remove_rule object=handler name=hash1 matcher=HASH index=0 alias=9999 key_len=7 key=blocked locator=key
ms_remove_rule：MS_SUCCESS
第二次 ms_find_rule：NO_MATCH
SESSION_RESULT commands_failed=0
```

这证明 HASH 规则由 `key + key_len` 定位，alias 不参与删除定位。

### TC-35-004：AC handler 按 alias + key 删除普通规则

类型：正常兼容接口用例。

```sh
$DEMO flow delete-handler-ac-rule > delete-handler-ac-rule.flow
cat delete-handler-ac-rule.flow
$DEMO session delete-handler-ac-rule.flow
```

流程明确删除：

```text
TARGET api=ms_remove_rule object=handler name=ach1 matcher=AC index=0 alias=101 key_len=3 key=abc locator=alias+key
```

预期删除前组合规则 `1001` 能命中；删除普通规则 `101` 后，依赖它的组合规则
也被清理，随后扫描为 `NO_MATCH`。

### TC-35-005：AC handler 按 alias 删除组合规则

类型：正常兼容接口用例。

```sh
$DEMO flow delete-handler-ac-combination > delete-handler-ac-combination.flow
cat delete-handler-ac-combination.flow
$DEMO session delete-handler-ac-combination.flow
```

删除参数必须是：

```text
TARGET api=ms_remove_rule object=handler name=ach1 matcher=AC index=0 alias=1001 key_len=0 key= locator=alias
```

预期删除前返回 `101,102,1001`，删除后只返回普通规则 `101,102`。

### TC-35-006：database 与 handler 删除入口不可混用

检查前面各流程中的第一个具名参数：

- `ms_remove_rule <handler> ...` 的对象必须是 `ms_handler_t` 句柄。
- `ms_remove_database_rule <database> ...` 的对象必须是 `ms_database_t` 句柄。
- HASH database 不支持 `ms_remove_database_rule`。
- 920HG 动态删除矩阵测试的是 database 正式路径，不代表 handler 兼容路径；
  handler 的三类语义由上述独立 flow 验证。

### TC-35-007：920HG 端到端删除矩阵

```sh
$DEMO 920hg quick --machine 920 --data-root /data/920hg_data_v4
```

预期 `raw_delete_trials.csv` 同时包含：

| algorithm | target_kind | locator | 含义 |
| --- | --- | --- | --- |
| `ac_delete` | `AC_NORMAL` | `alias+key` | 普通 AC database 删除普通规则 |
| `accombo_delete` | `AC_NORMAL_DEPENDENCY` | `alias+key` | ACCombo 删除普通依赖规则并清理组合 |
| `accombo_combination_delete` | `AC_COMBINATION` | `alias` | ACCombo 直接删除组合规则 |

每行还必须给出 `removed_alias`、`removed_key_len`、`removed_key` 和
`affected_id`。组合规则直接删除行中 `removed_key_len=0`、
`removed_key` 为空；其他两类必须记录实际 key。
`dynamic_delete_summary.csv` 也必须保留这四个字段，不能在汇总时只剩算法名。

每种 algorithm 的结果不能只显示一个总 `PASS`，还必须分别显示：

```text
failed_batch_rejected=PASS
failed_batch_unchanged=PASS
delete_success=PASS
repeat_delete_rejected=PASS
scratch_refresh=PASS
stream_stale=PASS
stream_reset=PASS
post_delete_block=PASS
post_delete_stream=PASS
semantic_effect=PASS
correctness=PASS
```

`run.log` 中对应输出格式为：

```text
DELETE_CHECK name=<检查项> expected=<预期> actual=<实际> status=PASS
```

这样失败时可以直接确定是删除调用、原子性、scratch、stream 还是删除后语义
出了问题，而不是只得到一个无法定位的总失败。

### TC-35-E001：删除后旧 stream stale，reset 后恢复

类型：包含预期 API 错误的状态用例。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc def '101&102' 2 2 1 101 102 1001 3
ms_alloc_scratch db1 scratch1
ms_open_stream db1 stream1
ms_scan_stream stream1 abc 3 scratch1 4
ms_remove_database_rule db1 101 3 abc 1
ms_scan_stream stream1 def 3 scratch1 4
ms_reset_stream stream1
ms_scan_stream stream1 def 3 scratch1 4
ms_close_stream stream1 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期整个 session 退出码：非 `0`，因为旧 stream 扫描预期返回：

```text
MS_STREAM_STALE
```

随后：

```text
ms_reset_stream ret=0(MS_SUCCESS)
```

reset 后扫描按删除后的规则集执行。

### TC-35-E002：database 批量删除原子失败

类型：预期失败，删除后状态保持。

```sh
$DEMO session <<'EOF'
ms_compile_database_multi db1 ac 0 0 abc def 0 0 101 102 2
ms_alloc_scratch db1 scratch1
ms_remove_database_rule db1 101 3 abc 999 3 xxx 2
ms_scan_database db1 abcdef 6 scratch1 4
ms_free_scratch scratch1
ms_free_database db1
exit
EOF
```

预期整个 session 退出码：非 `0`。

关键检查：

```text
批量删除失败
后续扫描仍返回 101,102
```

证明 database 在批量失败时保持不变。

### TC-35-E003：HASH database 不支持 ms_remove_database_rule

类型：预期失败。

```sh
$DEMO session <<'EOF'
ms_create_database db1 hash 16 4
ms_remove_database_rule db1 101 3 abc 1
ms_free_database db1
exit
EOF
```

预期：`MS_NOT_SUPPORTED`。

### TC-35-E004：rule_num 与规则数组数量不一致

类型：预期失败，CLI 参数校验。

```sh
$DEMO --api ms_remove_database_rule \
  101 3 abc \
  -- \
  0 \
  - 4 \
  ac 0 0 abc 0 101 1
```

预期：

```text
序列化了 1 条 rule，但 rule_num=0
```

目标删除接口不应调用。

## 8. C 单元测试

以下错误场景不能通过 Shell 安全、准确地传递真实 C 指针，必须运行 C 单元测试：

- NULL database、scratch、stream、ids、count
- 输出指针调用前非 NULL
- scratch 实际属于另一个 database
- stream 与 scratch 实际归属不一致
- allocator 第 N 次申请失败
- 中间资源失败后的释放
- handler 批量删除部分成功且不回滚
- stream 容量失败后的内部状态回滚
- database 删除失败后的原子保持
- database API 与 handler API 等价性

构建并运行：

```sh
cd /path/to/Megascan/test
python3 build.py testdemo_ut debug
./build/bin/testdemo_ut
echo $?
```

重点测试源码：

```text
test/src/ut/test_database_lifecycle.c
test/src/ut/test_database_block.c
test/src/ut/test_database_stream.c
test/src/ut/test_expr_compile.c
test/src/ut/test_expr_block_compat.c
test/src/ut/test_expr_stream_compat.c
test/src/ut/test_allocator_routing.c
test/src/ut/test_allocator_runtime_reuse.c
test/src/ut/test_ac_remove.c
test/src/ut/test_api_equivalence.c
```

## 9. 920hg 回归与性能测试

920hg 不用于替代错误参数测试，只用于真实数据正确性、单核和整机性能基线。

查看帮助：

```sh
$DEMO 920hg -h
$DEMO 920hg generate -h
$DEMO 920hg single -h
$DEMO 920hg full -h
$DEMO 920hg quick -h
```

Quick：

```sh
$DEMO 920hg quick \
  --machine 920 \
  --data-root /data/920hg_data_v4
```

单核：

```sh
$DEMO 920hg single \
  --machine 920 \
  --data-root /data/920hg_data_v4 \
  --cpu-core 1
```

整机：

```sh
$DEMO 920hg full \
  --machine 920 \
  --data-root /data/920hg_data_v4 \
  --cpu-list 1-95
```

## 10. 推荐执行顺序

```text
1. 构建 megascan_demo
2. 执行 TC-31 基础 database/scratch 用例
3. 执行 TC-32 组合规则及非法编译用例
4. 执行 TC-33 stream 生命周期、跨 chunk 和回滚用例
5. 执行 TC-34 allocator 路由用例
6. 执行 TC-35 删除、原子性和 stale/reset 用例
7. 运行 testdemo_ut 补齐指针、归属和故障注入
8. 最后运行 920hg quick、single 和 full
```

## 11. 当前测试能力边界

1. session 持久保存 database、scratch、database stream、handler、
   handler stream、logical tree/vector 和 allocator 状态。
2. logical vector 使用 `logical_vector_create/free` fixture 命令管理；
   `logical_tree_create_op` 为 `ms_logical_eval_inplace` 构造不含引用节点的
   直接操作树；真实 `ms_logical_*` 调用仍与公开签名一一对应。
3. allocator demo 尚不支持 `fail-after:N` 故障注入。
4. session 的 `OWNER_MISMATCH` 在 demo 层拦截，不会把错误对象传入 API。
5. NULL、悬空指针和输出指针初值错误必须由 C 单元测试覆盖。
6. 预期失败的 session 会累计 `commands_failed`，最终退出码非 0；判断测试结果时必须结合用例预期。

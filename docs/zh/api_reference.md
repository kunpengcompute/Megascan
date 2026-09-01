# API参考<a name="ZH-CN_TOPIC_0000002522230512"></a>

公开头文件位于`ms/include`。Database接口包含在`ms.h`中；使用handler兼容接口时按需包含`ms_handler.h`、`ms_rule.h`、`ms_compile.h`和`ms_runtime.h`；自定义内存分配器包含`ms_allocator.h`。

## 数据结构说明<a name="ZH-CN_TOPIC_0000002522070512"></a>

**匹配引擎类型**

```c
typedef enum {
    MS_MATCHER_NONE = 0,
    MS_MATCHER_TYPE_HASH,
    MS_MATCHER_TYPE_AC,
    MS_MATCHER_TYPE_MAX
} ms_matcher_type;
```

HASH用于定长key查找；AC支持普通规则、组合规则、Block扫描和Stream扫描。

>![](public_sys-resources/icon-note.gif) **说明：**
>
>`MS_MATCHER_TYPE_MAX`是枚举边界值，不能作为匹配引擎类型传入接口。

**错误码**

|错误码|返回值|解释|
|--|--|--|
|`MS_SUCCESS`|0|接口执行成功|
|`MS_INVALID_PARA`|-1|参数、对象归属或输出容量不合法|
|`MS_MALLOC_FAIL`|-2|内存申请失败|
|`MS_BUILT_ALREADY`|-3|对象已构建，不能重复构建|
|`MS_FIND_FAIL`|-4|扫描成功但没有可上报规则，或查找、删除目标不存在|
|`MS_INIT_RWLOCK_FAIL`|-5|读写锁初始化失败|
|`MS_MEMCPY_FAIL`|-6|内存复制失败|
|`MS_NOT_SUPPORTED`|-7|当前匹配类型不支持该操作|
|`MS_STREAM_STALE`|-8|database更新后，stream需要先reset|

>![](public_sys-resources/icon-note.gif) **说明：**
>
>`MS_FIND_FAIL`不一定表示接口执行异常。对于扫描接口，该返回值表示扫描已完成，但没有可上报的规则。

**规则标志**

|标志|说明|
|--|--|
|`MS_FLAG_NONE`|普通上报规则|
|`MS_FLAG_COMBINATION`|组合规则|
|`MS_FLAG_QUIET`|参与组合计算但不单独上报|
|`MS_FLAG_SINGLEMATCH`|同一规则只上报一次|

>![](public_sys-resources/icon-note.gif) **说明：**
>
>`MS_FLAG_QUIET`只抑制规则ID上报，不影响该规则参与组合规则计算。

**规则和扩展参数**

```c
typedef struct {
    uint32_t alias;
    uint32_t key_len;
    uint8_t *key;
} ms_rule_t;

typedef struct {
    uint32_t flags;
    uint64_t min_offset;
    uint64_t max_offset;
} ms_expr_ext_t;

typedef struct {
    uint32_t id;
    uint64_t from;
    uint64_t to;
} ms_match_event_t;
```

`ms_expr_ext_t.flags`可使用`MS_EXT_FLAG_MIN_OFFSET`和`MS_EXT_FLAG_MAX_OFFSET`。

`ms_match_event_t`表示普通规则的一次命中，位置范围采用`[from, to)`。

>![](public_sys-resources/icon-note.gif) **说明：**
>
>当前offset约束仅适用于组合规则，表示使组合规则成立的匹配结束位置。

**运行对象**

`ms_database_t`保存编译后的规则和匹配器，`ms_scratch_t`保存扫描工作区，`ms_stream_t`保存跨分片状态。三者均为不透明类型。

>![](public_sys-resources/icon-caution.gif) **注意：**
>
>不要直接访问、复制或自行释放不透明对象。对象应通过对应的创建、使用和释放接口管理，并遵守接口规定的归属关系。

## 使用须知<a name="ZH-CN_TOPIC_0000002522070513"></a>

>![](public_sys-resources/icon-caution.gif) **注意：**
>
>自定义allocator是进程级配置，应在创建Megascan对象和启动工作线程前完成。设置过程没有内部同步，不能与其他allocator setter或内存申请并发执行。
>
>每次内存申请都会在内部头部记录当时的free回调，因此更换或恢复allocator只影响后续申请，已有内存仍由原free回调释放。原free函数及其依赖的内存池必须保持有效，直至对应内存全部释放。

## Database编译接口<a name="ZH-CN_TOPIC_0000002522070514"></a>

**接口定义**

```c
int ms_create_database(ms_database_t **db, ms_matcher_type type,
    uint32_t buckets, uint32_t secondary_buckets);

int ms_compile_database_multi(ms_database_t **db, ms_matcher_type type,
    uint32_t buckets, uint32_t secondary_buckets,
    const char *const *expressions, const uint32_t *flags,
    const uint32_t *ids, uint32_t elements);

int ms_compile_database_multi_ext(ms_database_t **db, ms_matcher_type type,
    uint32_t buckets, uint32_t secondary_buckets,
    const char *const *expressions, const uint32_t *flags,
    const uint32_t *ids, const ms_expr_ext_t *ext, uint32_t elements);

int ms_free_database(ms_database_t *db);
```

**参数说明**

|参数名|描述|
|--|--|
|db|输出database指针，调用前应指向`NULL`|
|type|`MS_MATCHER_TYPE_HASH`或`MS_MATCHER_TYPE_AC`|
|buckets|HASH一级桶数量；AC可传0|
|secondary_buckets|HASH二级桶数量；AC可传0|
|expressions|长度为`elements`的表达式数组|
|flags|与表达式一一对应的规则标志数组|
|ids|与表达式一一对应的规则ID数组|
|ext|与表达式一一对应的扩展参数数组；不使用时可传`NULL`|
|elements|表达式、flags、ids和ext的元素数量|

`ms_compile_database_multi()`等价于将`ext`传为`NULL`后调用`ms_compile_database_multi_ext()`。组合表达式通过规则ID引用普通规则，例如`101&102`、`101|102`和`!101`。HASH database不支持组合规则。

## Scratch和Block扫描接口<a name="ZH-CN_TOPIC_0000002522070515"></a>

**接口定义**

```c
int ms_alloc_scratch(const ms_database_t *db, ms_scratch_t **scratch);
int ms_free_scratch(ms_scratch_t *scratch);

int ms_scan_database(const ms_database_t *db, const uint8_t *data,
    uint32_t len, ms_scratch_t *scratch, uint32_t *ids,
    uint32_t *id_count, uint32_t id_capacity);

int ms_get_match_events(const ms_scratch_t *scratch,
    ms_match_event_t *events, uint32_t *event_count,
    uint32_t event_capacity);

int ms_find_database_rule(const ms_database_t *db, ms_rule_t *rule);
```

`scratch`必须由同一个database创建。`ms_scan_database()`当前只支持AC database；它通过`ids`和`id_count`返回本次扫描需要上报的去重规则ID及数量。没有可上报ID时返回`MS_FIND_FAIL`。输出容量不足时返回`MS_INVALID_PARA`，并通过`id_count`返回所需容量。

扫描完成后可调用`ms_get_match_events()`读取该scratch最近一次扫描捕获的普通规则命中。每次命中单独返回，包含规则ID和`[from, to)`位置；Quiet规则和组合规则不在事件中。先将`events`传`NULL`、`event_capacity`传0获取所需数量，再按该数量分配数组并再次调用。

## Stream扫描接口<a name="ZH-CN_TOPIC_0000002522070516"></a>

**接口定义**

```c
int ms_open_stream(const ms_database_t *db, ms_stream_t **stream);

int ms_scan_stream(ms_stream_t *stream, const uint8_t *data,
    uint32_t len, ms_scratch_t *scratch, uint32_t *ids,
    uint32_t *id_count, uint32_t id_capacity);

int ms_flush_stream(ms_stream_t *stream, ms_scratch_t *scratch,
    uint32_t *ids, uint32_t *id_count, uint32_t id_capacity);

int ms_close_stream(ms_stream_t *stream, ms_scratch_t *scratch,
    uint32_t *ids, uint32_t *id_count, uint32_t id_capacity);

int ms_reset_stream(ms_stream_t *stream);
```

Stream模式仅支持AC database。stream保存自动机状态、累计offset和组合规则状态，因此规则可以跨任意数量的输入分片匹配；分片大小由调用者根据真实输入决定，Megascan不要求固定大小。`scratch`仍然是每次扫描、flush和close的共同前提，且必须属于该stream绑定的database。

`ms_flush_stream()`执行流结束语义但不关闭stream；`ms_close_stream()`执行流结束语义并释放stream；`ms_reset_stream()`清除已有跨片状态。单次调用只返回本次新产生的去重规则ID。每次`ms_scan_stream()`完成后，可通过`ms_get_match_events()`读取该分片新产生的普通规则命中，位置是从流起点计算的绝对位置。

## 动态删除接口<a name="ZH-CN_TOPIC_0000002522070517"></a>

**接口定义**

```c
int ms_remove_database_rule(ms_database_t *db,
    const ms_rule_t *rule, uint32_t rule_num);
```

普通AC规则使用`alias + key_len + key`定位；组合规则使用`alias`定位，此时`key=NULL`且`key_len=0`。删除普通规则时，同时删除依赖该规则的组合规则。批量删除具有原子性：任一目标不存在时，database保持不变。

删除成功后，已有scratch在下次扫描时自动刷新；已有stream返回`MS_STREAM_STALE`，调用`ms_reset_stream()`后按新database继续使用。HASH database不支持该接口。调用者必须在删除与并发扫描之间进行外部同步。

## Handler兼容接口<a name="ZH-CN_TOPIC_0000002522070518"></a>

Handler接口保留原有的可变对象调用方式。新接入代码建议优先使用database、scratch和stream接口，因为这些接口将规则库、扫描工作区和跨分片状态分别由database、scratch和stream显式管理，资源归属和生命周期更加清晰；并发扫描时也可以为每个线程分配独立scratch，避免共享可变的扫描状态。

```c
int ms_handler_init(ms_handler_t *handler, ms_matcher_type type,
    uint32_t buckets, uint32_t secondary_buckets);
int ms_handler_destroy(ms_handler_t *handler);
int ms_set_hash_func(ms_handler_t *handler, hash_calc_func hash_func);
int ms_add_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num);
int ms_remove_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num);
int ms_find_rule(ms_handler_t *handler, ms_rule_t *rule);
int ms_scan_rule_stream(ms_stream_t *stream, const uint8_t *data,
    uint32_t len);

int ms_compile_multi(ms_handler_t *handler,
    const char *const *expressions, const uint32_t *flags,
    const uint32_t *ids, uint32_t elements);
int ms_compile_multi_ext(ms_handler_t *handler,
    const char *const *expressions, const uint32_t *flags,
    const uint32_t *ids, const ms_expr_ext_t *ext, uint32_t elements);
int ms_add_exprs(ms_handler_t *handler, const char *const *expressions,
    const uint32_t *ids, const uint32_t *flags, uint32_t elements);

int ms_scan_expr(ms_handler_t *handler, const uint8_t *data, uint32_t len,
    uint32_t *ids, uint32_t *id_count, uint32_t id_capacity);
int ms_open_handler_stream(ms_handler_t *handler, ms_stream_t **stream);
int ms_scan_expr_stream(ms_stream_t *stream, const uint8_t *data,
    uint32_t len, uint32_t *ids, uint32_t *id_count,
    uint32_t id_capacity);
int ms_scan_expr_stream_flush(ms_stream_t *stream, uint32_t *ids,
    uint32_t *id_count, uint32_t id_capacity);
int ms_close_handler_stream(ms_stream_t *stream);
```

HASH handler按key查找规则；AC handler支持表达式编译、Block和Stream扫描。`ms_remove_rule()`删除HASH规则时按key定位；删除AC普通规则时按`alias + key`定位；删除AC组合规则时按alias定位。

## 组合辅助接口<a name="ZH-CN_TOPIC_0000002522070519"></a>

**接口定义**

```c
int ms_compile_eval_combinations(ms_handler_t *handler,
    const uint32_t *aliases, uint32_t alias_count,
    uint32_t *comb_ids, uint32_t *comb_count,
    uint32_t comb_capacity);

int ms_compile_scan_ac_combinations(ms_handler_t *handler,
    const uint8_t *data, uint32_t len,
    uint32_t *comb_ids, uint32_t *comb_count,
    uint32_t comb_capacity);
```

`ms_compile_eval_combinations()`根据已命中的普通规则alias计算组合规则ID；`ms_compile_scan_ac_combinations()`扫描一段完整数据并只返回组合规则ID。两者属于Handler兼容接口。

组合编译上下文由handler持有，并在`ms_handler_destroy()`中统一释放，调用方不需要单独管理。

## 逻辑表达式接口<a name="ZH-CN_TOPIC_0000002522070520"></a>

**接口定义**

```c
int ms_logical_parse(const char *logical, ms_logical_tree_t *tree);
void ms_logical_tree_destroy(ms_logical_tree_t *tree);
int ms_logical_set(uint8_t *logical_vector, uint32_t logical_count,
    uint32_t key, uint8_t value);
int ms_logical_get(const uint8_t *logical_vector, uint32_t logical_count,
    uint32_t key, uint8_t *value);
int ms_logical_clear(uint8_t *logical_vector, uint32_t logical_count);
int ms_logical_eval(const ms_logical_tree_t *tree,
    uint8_t *logical_vector, uint32_t logical_count, uint8_t *matched);
int ms_logical_eval_inplace(const ms_logical_tree_t *tree,
    uint8_t *logical_vector, uint32_t logical_count, uint8_t *matched);
```

`ms_logical_parse()`将包含`!`、`&`、`|`和括号的表达式解析为逻辑树；`ms_logical_set()`、`ms_logical_get()`和`ms_logical_clear()`管理规则状态向量；`ms_logical_eval()`和`ms_logical_eval_inplace()`计算表达式结果。逻辑树不再使用时调用`ms_logical_tree_destroy()`释放。

## 内存分配器接口<a name="ZH-CN_TOPIC_0000002522070521"></a>

**接口定义**

```c
int ms_set_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
    ms_free_t free_func);
int ms_set_database_allocator(ms_alloc_t alloc_func,
    ms_calloc_t calloc_func, ms_free_t free_func);
int ms_set_scratch_allocator(ms_alloc_t alloc_func,
    ms_calloc_t calloc_func, ms_free_t free_func);
int ms_set_stream_allocator(ms_alloc_t alloc_func,
    ms_calloc_t calloc_func, ms_free_t free_func);
int ms_set_misc_allocator(ms_alloc_t alloc_func,
    ms_calloc_t calloc_func, ms_free_t free_func);
```

|接口|作用范围|
|--|--|
|`ms_set_allocator()`|同时设置全部内存分类|
|`ms_set_database_allocator()`|database、HASH表、AC自动机和组合编译上下文|
|`ms_set_scratch_allocator()`|scratch、命中缓存和扫描工作区|
|`ms_set_stream_allocator()`|stream对象及跨分片状态|
|`ms_set_misc_allocator()`|杂项API内存|

三个回调参数依次对应`malloc`、`calloc`和`free`。回调传`NULL`时使用标准库默认实现。allocator的配置时机、释放回调记录和并发约束请参见[使用须知](#ZH-CN_TOPIC_0000002522070513)。

## 并发和资源释放约束<a name="ZH-CN_TOPIC_0000002522070522"></a>

- 不同database和不同stream可由调用者并发使用。
- 同一个scratch或stream不能在多个线程中无同步并发使用。
- 同一个database可被多个只读扫描共享，但每个并发扫描应使用独立scratch。
- 动态删除与扫描不能无同步并发执行。
- 推荐释放顺序为stream、scratch、database；handler路径先关闭stream，再销毁handler。

## 修订记录

| 文档版本 | 发布日期 | 修改说明 |
| -------- | -------- | -------- |
| 01 | 2026-09-30 | 第一次正式发布 |

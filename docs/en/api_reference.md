# API Reference<a name="ZH-CN_TOPIC_0000002522230512"></a>

<!-- md-trans-meta sourceCommit=cef0cf22901fa9ada7db6826684488ff749a38f9 translatedAt=2026-08-31T01:42:06.578Z pushedAt=2026-09-04T09:30:51.637Z -->

The public header files are located in `ms/include`. The database APIs are included in `ms.h`. To use the handler-compatible APIs, include `ms_handler.h`, `ms_rule.h`, `ms_compile.h`, and `ms_runtime.h` as needed. To customize a memory allocator, include `ms_allocator.h`.

## Data Structure Description<a name="ZH-CN_TOPIC_0000002522070512"></a>

- Matching engine types

    ```c
    typedef enum {
        MS_MATCHER_NONE = 0,
        MS_MATCHER_TYPE_HASH,
        MS_MATCHER_TYPE_AC,
        MS_MATCHER_TYPE_MAX
    } ms_matcher_type;
    ```

    The HASH type is used for fixed-length key lookup. The AC type supports basic rules, composite rules, block scanning, and stream scanning.

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >
    >`MS_MATCHER_TYPE_MAX` is an enumeration boundary value and cannot be passed to an API as a valid matching engine type.

- Error codes

    |Error Code|Return Value|Description|
    |--|--|--|
    |`MS_SUCCESS`|`0`|An API is successfully executed.|
    |`MS_INVALID_PARA`|`-1`|The parameter, object ownership, or output capacity is invalid.|
    |`MS_MALLOC_FAIL`|`-2`|Failed to allocate memory.|
    |`MS_BUILT_ALREADY`|`-3`|The object has already been built and cannot be built again.|
    |`MS_FIND_FAIL`|`-4`|The scanning is successful but there is no reportable rule, or the target to be queried or deleted does not exist.|
    |`MS_INIT_RWLOCK_FAIL`|`-5`|Failed to initialize the read-write lock.|
    |`MS_MEMCPY_FAIL`|`-6`|Failed to copy memory.|
    |`MS_NOT_SUPPORTED`|`-7`|The current matching type does not support this operation.|
    |`MS_STREAM_STALE`|`-8`|After the database is updated, the stream needs to be reset first.|

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >
    >`MS_FIND_FAIL` does not necessarily indicate abnormal API execution. For scanning APIs, this return value indicates that the scanning is complete but there is no reportable rule.

- Rule flags

    |Flag|Description|
    |--|--|
    |`MS_FLAG_NONE`|Indicates the basic rule.|
    |`MS_FLAG_COMBINATION`|Indicates the composite rule.|
    |`MS_FLAG_QUIET`|Indicates that the rule participates in composite calculation but is not reported individually.|
    |`MS_FLAG_SINGLEMATCH`|Indicates that the same rule is reported only once.|

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >
    >`MS_FLAG_QUIET` only suppresses rule ID reporting and does not affect the rule's participation in composite rule calculation.

- Rules and extension parameters

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
    ```

    `ms_expr_ext_t.flags` can use `MS_EXT_FLAG_MIN_OFFSET` and `MS_EXT_FLAG_MAX_OFFSET`.

    >![](public_sys-resources/icon-note.gif) **NOTE:**
    >
    >The current offset constraint applies only to composite rules, and it indicates the matching end position at which the composite rule is satisfied.

- Running objects

    `ms_database_t` stores compiled rules and matchers, `ms_scratch_t` stores the scanning workspace, and `ms_stream_t` stores cross-segment state. All three are opaque types.

    >![](public_sys-resources/icon-caution.gif) **CAUTION:**
    >
    >Do not directly access, copy, or free opaque objects. Objects should be managed through the corresponding creation, use, and release APIs, and the ownership specified by the APIs must be observed.

## Usage Notes<a id="usage-notes"></a>

>![](public_sys-resources/icon-caution.gif) **CAUTION:**
>
>A custom allocator is a process-level configuration and must be set before any Megascan object is created or any worker thread is started. The setting process has no internal synchronization and cannot run concurrently with other allocator setters or memory allocations.
>
>Each memory allocation records the free callback at that time in an internal header. Therefore, replacing or restoring the allocator affects only subsequent allocations; existing memory is still released by the original free callback. The original free function and the memory pool it depends on must remain valid until all corresponding memory is released.

## Database Compilation APIs

**API definition**

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

**Parameter description**

|Parameter|Description|
|--|--|
|db|Output database pointer. It should point to `NULL` before the API call.|
|type|`MS_MATCHER_TYPE_HASH` or `MS_MATCHER_TYPE_AC`.|
|buckets|Number of level-1 buckets for HASH; `0` can be passed for AC.|
|secondary_buckets|Number of level-2 buckets for HASH; `0` can be passed for AC.|
|expressions|Expression array with a length of `elements`.|
|flags|Rule flag array in one-to-one correspondence with `expressions`.|
|ids|Rule ID array in one-to-one correspondence with `expressions`.|
|ext|Extension parameter array in one-to-one correspondence with `expressions`; `NULL` can be passed if this parameter is not used.|
|elements|Number of elements in `expressions`, `flags`, `ids`, and `ext`.|

`ms_compile_database_multi()` is equivalent to `ms_compile_database_multi_ext()` with `ext` set to `NULL`. Composite expressions reference basic rules by rule ID, for example `101&102`, `101|102`, and `!101`. HASH databases do not support composite rules.

## Scratch and Block Scanning APIs

**API definition**

```c
int ms_alloc_scratch(const ms_database_t *db, ms_scratch_t **scratch);
int ms_free_scratch(ms_scratch_t *scratch);

int ms_scan_database(const ms_database_t *db, const uint8_t *data,
    uint32_t len, ms_scratch_t *scratch, uint32_t *ids,
    uint32_t *id_count, uint32_t id_capacity);

int ms_find_database_rule(const ms_database_t *db, ms_rule_t *rule);
```

`scratch` must be created by the same database. `ms_scan_database()` currently supports only AC databases; it returns the deduplicated rule IDs and count that need to be reported for this scanning, but does not return the start and end positions of each match or the raw match count. When there are no reportable IDs, it returns `MS_FIND_FAIL`. When the output capacity is insufficient, it returns `MS_INVALID_PARA` and also returns the required capacity via `id_count`.

## Stream Scanning APIs

**API definition**

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

The stream mode supports only AC databases. A stream preserves automaton state, accumulated offset, and composite rule state, so rules can match across any number of input segments; the segment size is determined by the caller based on the actual input, and Megascan does not require a fixed size. `scratch` remains the common prerequisite for each scan, flush, and close API call, and must belong to the database bound to that stream.

`ms_flush_stream()` performs stream end semantics but does not close the stream; `ms_close_stream()` performs stream end semantics and releases the stream; `ms_reset_stream()` clears existing cross-segment state. A single call returns only the deduplicated rule IDs newly produced by this call.

## Dynamic Deletion API

**API definition**

```c
int ms_remove_database_rule(ms_database_t *db,
    const ms_rule_t *rule, uint32_t rule_num);
```

A basic AC rule is located by `alias + key_len + key`; a composite rule is located by `alias`, in which case `key=NULL` and `key_len=0`. When a basic rule is deleted, the composite rules that depend on it are deleted as well. Batch deletion is atomic: if any target does not exist, the database remains unchanged.

After a successful deletion, existing scratches are automatically refreshed on the next scanning; existing streams return `MS_STREAM_STALE`, and `ms_reset_stream()` is called to use the new database. HASH databases do not support this API. The caller must perform external synchronization between deletion and concurrent scanning.

## Handler-compatible APIs

Handler APIs retain the original mutable-object calling style. For newly integrated code, it is recommended that the database, scratch, and stream APIs be used, because these APIs explicitly manage the rule library, scanning workspace, and cross-segment state through databases, scratches, and streams, respectively, making resource ownership and lifecycle clearer. During concurrent scanning, an independent scratch can also be allocated for each thread to avoid sharing mutable scanning states.

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

The HASH handler looks up rules by key; the AC handler supports expression compilation, block scanning, and stream scanning. `ms_remove_rule()` locates hash rules by `key` when deleting them; locates basic AC rules by `alias + key` when deleting them; and locates AC composite rules by `alias` when deleting them.

## Memory Allocator APIs

**API definition**

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

`ms_set_allocator()` sets all categories at once; the other APIs control the database, scratch, stream, and misc memory, respectively. When `NULL` is passed as the callback, the default implementation of the standard library is used. For the configuration timing, callback record release, and concurrency constraints of allocators, see [Usage Notes](#usage-notes).

## Concurrency and Resource Release Constraints

- Different databases and different streams can be used concurrently by the caller.

- The same scratch or stream cannot be used concurrently by multiple threads without synchronization.

- The same database can be shared by multiple read-only scans, but each concurrent scan should use an independent scratch.

- Dynamic deletion and scanning cannot be executed concurrently without synchronization.

- The recommended release order is stream, scratch, and database; for the handler path, close the stream first, and then destroy the handler.
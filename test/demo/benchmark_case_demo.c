/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Run one query from the 920hg benchmark data set.
 */
#include "benchmark_case_demo.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo_common.h"
#include "ms.h"
#include "ms_compile.h"
#include "ms_handler.h"
#include "ms_rule.h"
#include "ms_runtime.h"

#define DEMO_CORPUS_MAGIC 0x4d534243u
#define DEMO_CORPUS_VERSION 1u
#define DEMO_KIND_HASH 1u
#define DEMO_KIND_AC 2u
#define DEMO_KIND_ACCOMBO 3u
#define DEMO_HASH_BATCH 65536u
#define DEMO_OUTPUT_CAPACITY 64u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t record_header_size;
    uint32_t query_count;
    uint32_t payload_size;
    uint32_t hit_count;
    uint32_t seed;
    uint32_t kind;
    uint32_t reserved;
} demo_corpus_header_t;

typedef struct {
    uint32_t expected_ids;
    uint32_t expected_candidates;
    uint32_t stream_id;
    uint32_t reserved;
} demo_record_header_t;

typedef struct {
    uint32_t src_ip;
    uint16_t src_port;
    uint8_t protocol;
    uint32_t dst_ip;
    uint16_t dst_port;
} demo_tuple_t;

typedef struct {
    char **expressions;
    uint32_t *flags;
    uint32_t *ids;
    uint32_t count;
} demo_expression_set_t;

typedef struct {
    demo_corpus_header_t header;
    uint8_t *records;
    size_t record_size;
} demo_corpus_t;

_Static_assert(sizeof(demo_corpus_header_t) == 40, "corpus header layout changed");
_Static_assert(sizeof(demo_record_header_t) == 16, "corpus record layout changed");
_Static_assert(sizeof(demo_tuple_t) == 16, "tuple layout changed");

static char *duplicate_string(const char *text)
{
    size_t size = strlen(text) + 1u;
    char *copy = (char *)malloc(size);

    if (copy != NULL) {
        (void)memcpy(copy, text, size);
    }
    return copy;
}

static void free_expressions(demo_expression_set_t *set)
{
    uint32_t i;

    for (i = 0; i < set->count; i++) {
        free(set->expressions[i]);
    }
    free(set->expressions);
    free(set->flags);
    free(set->ids);
    (void)memset(set, 0, sizeof(*set));
}

static int load_expressions(const char *path, uint32_t count,
                            demo_expression_set_t *set)
{
    FILE *fp = fopen(path, "r");
    char line[4096];

    if (fp == NULL) {
        printf("无法打开规则文件：%s\n", path);
        return -1;
    }
    set->expressions = (char **)calloc(count, sizeof(*set->expressions));
    set->flags = (uint32_t *)calloc(count, sizeof(*set->flags));
    set->ids = (uint32_t *)calloc(count, sizeof(*set->ids));
    if (set->expressions == NULL || set->flags == NULL || set->ids == NULL) {
        printf("无法为规则分配内存。\n");
        (void)fclose(fp);
        free_expressions(set);
        return -1;
    }

    while (set->count < count && fgets(line, sizeof(line), fp) != NULL) {
        char *tab1 = strchr(line, '\t');
        char *tab2 = tab1 == NULL ? NULL : strchr(tab1 + 1, '\t');
        char *end;

        if (tab1 == NULL || tab2 == NULL) {
            goto invalid;
        }
        *tab1 = '\0';
        *tab2 = '\0';
        end = tab2 + 1 + strlen(tab2 + 1);
        while (end > tab2 + 1 && (end[-1] == '\n' || end[-1] == '\r')) {
            *--end = '\0';
        }
        if (tab2[1] == '\0') {
            goto invalid;
        }
        set->ids[set->count] = (uint32_t)strtoul(line, NULL, 10);
        set->flags[set->count] = (uint32_t)strtoul(tab1 + 1, NULL, 10);
        set->expressions[set->count] = duplicate_string(tab2 + 1);
        if (set->expressions[set->count] == NULL) {
            goto invalid;
        }
        set->count++;
    }
    (void)fclose(fp);
    if (set->count == count) {
        return 0;
    }
    printf("规则文件中的规则数量不足：%s\n", path);
    free_expressions(set);
    return -1;

invalid:
    printf("规则文件格式错误或规则数量不足：%s\n", path);
    (void)fclose(fp);
    free_expressions(set);
    return -1;
}

static int load_query(const char *path, uint32_t query_index,
                      demo_corpus_header_t *header,
                      demo_record_header_t *record, uint8_t **payload)
{
    FILE *fp = fopen(path, "rb");
    uint64_t offset;

    if (fp == NULL || fread(header, sizeof(*header), 1, fp) != 1 ||
        header->magic != DEMO_CORPUS_MAGIC ||
        header->version != DEMO_CORPUS_VERSION ||
        header->header_size != sizeof(*header) ||
        header->record_header_size != sizeof(*record) ||
        header->payload_size == 0 || query_index >= header->query_count) {
        printf("corpus 文件无效或查询序号越界：%s\n", path);
        if (fp != NULL) {
            (void)fclose(fp);
        }
        return -1;
    }
    offset = sizeof(*header) + (uint64_t)query_index *
             (sizeof(*record) + header->payload_size);
    if (offset > (uint64_t)LONG_MAX || fseek(fp, (long)offset, SEEK_SET) != 0) {
        printf("无法定位 corpus 查询：%u\n", query_index);
        (void)fclose(fp);
        return -1;
    }
    *payload = (uint8_t *)malloc(header->payload_size);
    if (*payload == NULL || fread(record, sizeof(*record), 1, fp) != 1 ||
        fread(*payload, header->payload_size, 1, fp) != 1) {
        printf("无法读取 corpus 查询：%u\n", query_index);
        free(*payload);
        *payload = NULL;
        (void)fclose(fp);
        return -1;
    }
    (void)fclose(fp);
    return 0;
}

static int load_corpus(const char *path, demo_corpus_t *corpus)
{
    FILE *fp = fopen(path, "rb");
    uint64_t data_size;

    if (fp == NULL || fread(&corpus->header, sizeof(corpus->header), 1, fp) != 1 ||
        corpus->header.magic != DEMO_CORPUS_MAGIC ||
        corpus->header.version != DEMO_CORPUS_VERSION ||
        corpus->header.header_size != sizeof(corpus->header) ||
        corpus->header.record_header_size != sizeof(demo_record_header_t) ||
        corpus->header.query_count == 0 || corpus->header.payload_size == 0) {
        printf("corpus 文件无效：%s\n", path);
        if (fp != NULL) {
            (void)fclose(fp);
        }
        return -1;
    }
    corpus->record_size = sizeof(demo_record_header_t) + corpus->header.payload_size;
    data_size = (uint64_t)corpus->record_size * corpus->header.query_count;
    if (data_size > SIZE_MAX) {
        (void)fclose(fp);
        return -1;
    }
    corpus->records = (uint8_t *)malloc((size_t)data_size);
    if (corpus->records == NULL ||
        fread(corpus->records, (size_t)data_size, 1, fp) != 1) {
        printf("无法读取完整 corpus：%s\n", path);
        free(corpus->records);
        corpus->records = NULL;
        (void)fclose(fp);
        return -1;
    }
    (void)fclose(fp);
    return 0;
}

static const demo_record_header_t *corpus_record(const demo_corpus_t *corpus,
                                                  uint32_t index)
{
    return (const demo_record_header_t *)(corpus->records +
                                           (size_t)index * corpus->record_size);
}

static const uint8_t *corpus_payload(const demo_corpus_t *corpus, uint32_t index)
{
    return corpus->records + (size_t)index * corpus->record_size +
           sizeof(demo_record_header_t);
}

static int load_hash_rules(const char *path, uint32_t count, demo_tuple_t **rules)
{
    FILE *fp = fopen(path, "rb");

    *rules = (demo_tuple_t *)malloc((size_t)count * sizeof(**rules));
    if (fp == NULL || *rules == NULL ||
        fread(*rules, sizeof(**rules), count, fp) != count) {
        printf("无法读取 HASH 规则文件：%s\n", path);
        if (fp != NULL) {
            (void)fclose(fp);
        }
        free(*rules);
        *rules = NULL;
        return -1;
    }
    (void)fclose(fp);
    return 0;
}

static int add_hash_rules(ms_handler_t *handler, demo_tuple_t *rules, uint32_t count)
{
    ms_rule_t *batch = (ms_rule_t *)malloc(DEMO_HASH_BATCH * sizeof(*batch));
    uint32_t offset;

    if (batch == NULL) {
        return -1;
    }
    for (offset = 0; offset < count; offset += DEMO_HASH_BATCH) {
        uint32_t batch_count = count - offset;
        uint32_t i;
        int ret;

        if (batch_count > DEMO_HASH_BATCH) {
            batch_count = DEMO_HASH_BATCH;
        }
        for (i = 0; i < batch_count; i++) {
            batch[i].alias = offset + i + 1u;
            batch[i].key_len = sizeof(*rules);
            batch[i].key = (uint8_t *)&rules[offset + i];
        }
        ret = ms_add_rule(handler, batch, batch_count);
        if (ret != MS_SUCCESS) {
            printf("添加 HASH 规则失败：offset=%u，返回值=%d\n", offset, ret);
            free(batch);
            return -1;
        }
    }
    free(batch);
    return 0;
}

static int add_legacy_rules(ms_handler_t *handler, const demo_expression_set_t *set)
{
    ms_rule_t *rules = (ms_rule_t *)calloc(set->count, sizeof(*rules));
    uint32_t i;
    int ret;

    if (rules == NULL) {
        return -1;
    }
    for (i = 0; i < set->count; i++) {
        rules[i].alias = set->ids[i];
        rules[i].key = (uint8_t *)set->expressions[i];
        rules[i].key_len = (uint32_t)strlen(set->expressions[i]);
    }
    ret = ms_add_rule(handler, rules, set->count);
    free(rules);
    return ret;
}

static int compile_atomic_database(ms_database_t **db,
                                   const demo_expression_set_t *set)
{
    const char **expressions;
    uint32_t *flags;
    uint32_t *ids;
    uint32_t count = 0;
    uint32_t i;
    int ret;

    expressions = (const char **)calloc(set->count, sizeof(*expressions));
    flags = (uint32_t *)calloc(set->count, sizeof(*flags));
    ids = (uint32_t *)calloc(set->count, sizeof(*ids));
    if (expressions == NULL || flags == NULL || ids == NULL) {
        free(expressions);
        free(flags);
        free(ids);
        return -1;
    }
    for (i = 0; i < set->count; i++) {
        if ((set->flags[i] & MS_FLAG_COMBINATION) == 0) {
            expressions[count] = set->expressions[i];
            flags[count] = set->flags[i] & ~MS_FLAG_QUIET;
            ids[count] = set->ids[i];
            count++;
        }
    }
    ret = count == 0 ? MS_INVALID_PARA :
          ms_compile_database_multi(db, MS_MATCHER_TYPE_AC, 0, 0,
                                    (const char *const *)expressions,
                                    flags, ids, count);
    free(expressions);
    free(flags);
    free(ids);
    return ret;
}

static int run_hash_case(const demo_benchmark_case_t *config,
                         const uint8_t *payload, uint32_t payload_size,
                         uint32_t *actual)
{
    ms_handler_t handler = {0};
    demo_tuple_t *rules = NULL;
    ms_rule_t query = {0, payload_size, (uint8_t *)payload};
    int initialized = 0;
    int ret;

    if (payload_size != sizeof(demo_tuple_t) ||
        load_hash_rules(config->rules_file, config->rule_count, &rules) != 0) {
        return -1;
    }
    ret = demo_step_ret("创建 HASH handler",
                        ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0));
    if (ret == MS_SUCCESS) {
        initialized = 1;
        ret = add_hash_rules(&handler, rules, config->rule_count);
    }
    if (ret == MS_SUCCESS) {
        ret = demo_step_ret("查询 corpus 中的 HASH key", ms_find_rule(&handler, &query));
        *actual = ret == MS_SUCCESS ? 1u : 0u;
        if (ret == MS_FIND_FAIL) {
            ret = MS_SUCCESS;
        }
    }
    if (initialized) {
        (void)demo_step_ret("销毁 HASH handler", ms_handler_destroy(&handler));
    }
    free(rules);
    return ret == MS_SUCCESS ? 0 : -1;
}

static int scan_database_payload(const demo_benchmark_case_t *config,
                                 ms_database_t *db, ms_scratch_t *scratch,
                                 ms_stream_t *stream, const uint8_t *payload,
                                 uint32_t payload_size, uint32_t *ids,
                                 uint32_t *actual);
static int scan_legacy_payload(const demo_benchmark_case_t *config,
                               ms_handler_t *handler, ms_stream_t *stream,
                               const uint8_t *payload, uint32_t payload_size,
                               uint32_t *ids, uint32_t *actual);

static int run_ac_case(const demo_benchmark_case_t *config,
                       const uint8_t *payload, uint32_t payload_size,
                       uint32_t *actual)
{
    demo_expression_set_t set = {0};
    ms_handler_t handler = {0};
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t ids[DEMO_OUTPUT_CAPACITY] = {0};
    int legacy = strcmp(config->algorithm, "ac_legacy") == 0;
    int atomic_only = strcmp(config->algorithm, "ac_on_combo") == 0;
    int stream_mode = strcmp(config->mode, "stream-full") == 0;
    int handler_initialized = 0;
    int ret = MS_SUCCESS;

    if (load_expressions(config->rules_file, config->rule_count, &set) != 0) {
        return -1;
    }
    if (legacy) {
        ret = demo_step_ret("创建旧版 AC handler",
                            ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0));
        if (ret == MS_SUCCESS) {
            handler_initialized = 1;
            ret = demo_step_ret("加载旧版 AC 规则", add_legacy_rules(&handler, &set));
        }
        if (ret == MS_SUCCESS && stream_mode) {
            ret = demo_step_ret("打开旧版 handler stream",
                                ms_open_handler_stream(&handler, &stream));
        }
        if (ret == MS_SUCCESS) {
            ret = scan_legacy_payload(config, &handler, stream, payload,
                                      payload_size, ids, actual);
            (void)demo_step_ret("扫描指定 corpus 查询", ret);
        }
    } else {
        ret = atomic_only ? compile_atomic_database(&db, &set) :
              ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                        (const char *const *)set.expressions,
                                        set.flags, set.ids, set.count);
        (void)demo_step_ret("编译 AC database", ret);
        if (ret == MS_SUCCESS) {
            ret = demo_step_ret("分配 scratch", ms_alloc_scratch(db, &scratch));
        }
        if (ret == MS_SUCCESS && stream_mode) {
            ret = demo_step_ret("打开 database stream", ms_open_stream(db, &stream));
        }
        if (ret == MS_SUCCESS) {
            ret = scan_database_payload(config, db, scratch, stream, payload,
                                        payload_size, ids, actual);
            (void)demo_step_ret("扫描指定 corpus 查询", ret);
        }
    }
    demo_print_ids("扫描结果", ids,
                   *actual < DEMO_OUTPUT_CAPACITY ? *actual : DEMO_OUTPUT_CAPACITY);

    if (stream != NULL) {
        if (legacy) {
            (void)demo_step_ret("关闭旧版 handler stream", ms_close_handler_stream(stream));
        } else {
            uint32_t close_count = 0;
            (void)demo_step_ret("关闭 database stream",
                                ms_close_stream(stream, scratch, ids, &close_count,
                                                DEMO_OUTPUT_CAPACITY));
        }
    }
    if (scratch != NULL) {
        (void)demo_step_ret("释放 scratch", ms_free_scratch(scratch));
    }
    if (db != NULL) {
        (void)demo_step_ret("释放 database", ms_free_database(db));
    }
    if (handler_initialized) {
        (void)demo_step_ret("销毁旧版 AC handler", ms_handler_destroy(&handler));
    }
    free_expressions(&set);
    return ret == MS_SUCCESS || ret == MS_FIND_FAIL ? 0 : -1;
}

static int scan_database_payload(const demo_benchmark_case_t *config,
                                 ms_database_t *db, ms_scratch_t *scratch,
                                 ms_stream_t *stream, const uint8_t *payload,
                                 uint32_t payload_size, uint32_t *ids,
                                 uint32_t *actual)
{
    uint32_t offset = 0;
    int stream_mode = strcmp(config->mode, "stream-full") == 0;

    *actual = 0;
    if (!stream_mode) {
        return ms_scan_database(db, payload, payload_size, scratch, ids, actual,
                                DEMO_OUTPUT_CAPACITY);
    }
    if (ms_reset_stream(stream) != MS_SUCCESS) {
        return MS_STREAM_STALE;
    }
    while (offset < payload_size) {
        uint32_t remaining = payload_size - offset;
        uint32_t length = config->chunk_size == 0 || config->chunk_size > remaining ?
                          remaining : config->chunk_size;
        uint32_t chunk_ids[DEMO_OUTPUT_CAPACITY];
        uint32_t count = 0;
        int ret = ms_scan_stream(stream, payload + offset, length, scratch,
                                 chunk_ids, &count, DEMO_OUTPUT_CAPACITY);
        uint32_t copy_count;

        if (ret != MS_SUCCESS && ret != MS_FIND_FAIL) {
            return ret;
        }
        copy_count = *actual >= DEMO_OUTPUT_CAPACITY ? 0 :
                     (count < DEMO_OUTPUT_CAPACITY - *actual ?
                      count : DEMO_OUTPUT_CAPACITY - *actual);
        if (copy_count > 0) {
            (void)memcpy(ids + *actual, chunk_ids,
                         (size_t)copy_count * sizeof(*ids));
        }
        *actual += count;
        offset += length;
    }
    return *actual == 0 ? MS_FIND_FAIL : MS_SUCCESS;
}

static int scan_legacy_payload(const demo_benchmark_case_t *config,
                               ms_handler_t *handler, ms_stream_t *stream,
                               const uint8_t *payload, uint32_t payload_size,
                               uint32_t *ids, uint32_t *actual)
{
    uint32_t offset = 0;
    int stream_mode = strcmp(config->mode, "stream-full") == 0;

    *actual = 0;
    if (!stream_mode) {
        return ms_scan_expr(handler, payload, payload_size, ids, actual,
                            DEMO_OUTPUT_CAPACITY);
    }
    if (ms_reset_stream(stream) != MS_SUCCESS) {
        return MS_STREAM_STALE;
    }
    while (offset < payload_size) {
        uint32_t remaining = payload_size - offset;
        uint32_t length = config->chunk_size == 0 || config->chunk_size > remaining ?
                          remaining : config->chunk_size;
        uint32_t chunk_ids[DEMO_OUTPUT_CAPACITY];
        uint32_t count = 0;
        int ret = ms_scan_expr_stream(stream, payload + offset, length,
                                      chunk_ids, &count, DEMO_OUTPUT_CAPACITY);
        uint32_t copy_count;

        if (ret != MS_SUCCESS && ret != MS_FIND_FAIL) {
            return ret;
        }
        copy_count = *actual >= DEMO_OUTPUT_CAPACITY ? 0 :
                     (count < DEMO_OUTPUT_CAPACITY - *actual ?
                      count : DEMO_OUTPUT_CAPACITY - *actual);
        if (copy_count > 0) {
            (void)memcpy(ids + *actual, chunk_ids,
                         (size_t)copy_count * sizeof(*ids));
        }
        *actual += count;
        offset += length;
    }
    return *actual == 0 ? MS_FIND_FAIL : MS_SUCCESS;
}

static int run_full_hash_corpus(const demo_benchmark_case_t *config,
                                const demo_corpus_t *corpus)
{
    ms_handler_t handler = {0};
    demo_tuple_t *rules = NULL;
    uint32_t passed = 0;
    uint32_t failed = 0;
    uint32_t i;
    int initialized = 0;
    int ret;

    if (corpus->header.payload_size != sizeof(demo_tuple_t) ||
        load_hash_rules(config->rules_file, config->rule_count, &rules) != 0) {
        return 1;
    }
    ret = demo_step_ret("创建 HASH handler",
                        ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0));
    if (ret == MS_SUCCESS) {
        initialized = 1;
        ret = add_hash_rules(&handler, rules, config->rule_count);
    }
    for (i = 0; ret == MS_SUCCESS && i < corpus->header.query_count; i++) {
        const demo_record_header_t *record = corpus_record(corpus, i);
        ms_rule_t query = {0, corpus->header.payload_size,
                           (uint8_t *)corpus_payload(corpus, i)};
        uint32_t actual;

        ret = ms_find_rule(&handler, &query);
        actual = ret == MS_SUCCESS ? 1u : 0u;
        if (ret != MS_SUCCESS && ret != MS_FIND_FAIL) {
            break;
        }
        if (actual == record->expected_ids) {
            passed++;
        } else {
            failed++;
        }
        ret = MS_SUCCESS;
    }
    if (initialized) {
        (void)demo_step_ret("销毁 HASH handler", ms_handler_destroy(&handler));
    }
    free(rules);
    printf("完整 corpus 汇总：查询=%u，通过=%u，失败=%u\n",
           corpus->header.query_count, passed, failed);
    return ret == MS_SUCCESS && failed == 0 ? 0 : 1;
}

static int run_full_ac_corpus(const demo_benchmark_case_t *config,
                              const demo_corpus_t *corpus)
{
    demo_expression_set_t set = {0};
    ms_handler_t handler = {0};
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t passed = 0;
    uint32_t failed = 0;
    uint64_t actual_ids = 0;
    uint64_t expected_ids = 0;
    uint32_t i;
    int legacy = strcmp(config->algorithm, "ac_legacy") == 0;
    int atomic_only = strcmp(config->algorithm, "ac_on_combo") == 0;
    int stream_mode = strcmp(config->mode, "stream-full") == 0;
    int initialized = 0;
    int ret;

    if (load_expressions(config->rules_file, config->rule_count, &set) != 0) {
        return 1;
    }
    if (legacy) {
        ret = demo_step_ret("创建旧版 AC handler",
                            ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0));
        if (ret == MS_SUCCESS) {
            initialized = 1;
            ret = demo_step_ret("加载旧版 AC 规则", add_legacy_rules(&handler, &set));
        }
        if (ret == MS_SUCCESS && stream_mode) {
            ret = demo_step_ret("打开旧版 handler stream",
                                ms_open_handler_stream(&handler, &stream));
        }
    } else {
        ret = atomic_only ? compile_atomic_database(&db, &set) :
              ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                        (const char *const *)set.expressions,
                                        set.flags, set.ids, set.count);
        (void)demo_step_ret("编译 AC database", ret);
        if (ret == MS_SUCCESS) {
            ret = demo_step_ret("分配 scratch", ms_alloc_scratch(db, &scratch));
        }
        if (ret == MS_SUCCESS && stream_mode) {
            ret = demo_step_ret("打开 database stream", ms_open_stream(db, &stream));
        }
    }

    for (i = 0; (ret == MS_SUCCESS || ret == MS_FIND_FAIL) &&
                i < corpus->header.query_count; i++) {
        const demo_record_header_t *record = corpus_record(corpus, i);
        const uint8_t *payload = corpus_payload(corpus, i);
        uint32_t expected = atomic_only ? record->expected_candidates : record->expected_ids;
        uint32_t ids[DEMO_OUTPUT_CAPACITY];
        uint32_t actual = 0;

        ret = legacy ?
              scan_legacy_payload(config, &handler, stream, payload,
                                  corpus->header.payload_size, ids, &actual) :
              scan_database_payload(config, db, scratch, stream, payload,
                                    corpus->header.payload_size, ids, &actual);
        if (ret != MS_SUCCESS && ret != MS_FIND_FAIL) {
            printf("扫描失败：查询序号=%u，返回值=%d\n", i, ret);
            break;
        }
        actual_ids += actual;
        expected_ids += expected;
        if (actual == expected) {
            passed++;
        } else {
            failed++;
        }
    }

    if (stream != NULL) {
        if (legacy) {
            (void)demo_step_ret("关闭旧版 handler stream", ms_close_handler_stream(stream));
        } else {
            uint32_t count = 0;
            uint32_t ids[DEMO_OUTPUT_CAPACITY];
            (void)demo_step_ret("关闭 database stream",
                                ms_close_stream(stream, scratch, ids, &count,
                                                DEMO_OUTPUT_CAPACITY));
        }
    }
    if (scratch != NULL) {
        (void)demo_step_ret("释放 scratch", ms_free_scratch(scratch));
    }
    if (db != NULL) {
        (void)demo_step_ret("释放 database", ms_free_database(db));
    }
    if (initialized) {
        (void)demo_step_ret("销毁旧版 AC handler", ms_handler_destroy(&handler));
    }
    free_expressions(&set);
    printf("完整 corpus 汇总：查询=%u，通过=%u，失败=%u，期望ID=%llu，实际ID=%llu\n",
           corpus->header.query_count, passed, failed,
           (unsigned long long)expected_ids, (unsigned long long)actual_ids);
    return (ret == MS_SUCCESS || ret == MS_FIND_FAIL) && failed == 0 ? 0 : 1;
}

static int run_full_corpus(const demo_benchmark_case_t *config)
{
    demo_corpus_t corpus = {0};
    uint32_t expected_kind;
    int hash = strcmp(config->algorithm, "hash") == 0;
    int ret;

    if (load_corpus(config->corpus_file, &corpus) != 0) {
        return 1;
    }
    expected_kind = hash ? DEMO_KIND_HASH :
                    strcmp(config->algorithm, "accombo") == 0 ||
                    strcmp(config->algorithm, "ac_on_combo") == 0 ?
                    DEMO_KIND_ACCOMBO : DEMO_KIND_AC;
    if (corpus.header.kind != expected_kind) {
        printf("算法与 corpus 类型不匹配：corpus kind=%u\n", corpus.header.kind);
        free(corpus.records);
        return 1;
    }
    printf("920hg 完整数据场景：算法=%s，模式=%s，规则数=%u，查询数=%u\n",
           config->algorithm, config->mode, config->rule_count,
           corpus.header.query_count);
    ret = hash ? run_full_hash_corpus(config, &corpus) :
                 run_full_ac_corpus(config, &corpus);
    free(corpus.records);
    return ret;
}

int demo_run_rule_file_scan(const demo_benchmark_case_t *config,
                            const uint8_t *input, uint32_t input_len)
{
    uint32_t actual = 0;
    int hash;
    int ret;

    if (config == NULL || config->algorithm == NULL || config->mode == NULL ||
        config->rules_file == NULL || config->rule_count == 0 || input == NULL) {
        printf("规则文件扫描参数不完整。\n");
        return 1;
    }
    hash = strcmp(config->algorithm, "hash") == 0;
    if ((!hash && strcmp(config->algorithm, "ac") != 0 &&
         strcmp(config->algorithm, "accombo") != 0 &&
         strcmp(config->algorithm, "ac_legacy") != 0 &&
         strcmp(config->algorithm, "ac_on_combo") != 0) ||
        (hash && strcmp(config->mode, "lookup") != 0) ||
        (!hash && strcmp(config->mode, "block") != 0 &&
         strcmp(config->mode, "stream-full") != 0)) {
        printf("算法或扫描模式不匹配。\n");
        return 1;
    }
    printf("规则文件扫描：算法=%s，模式=%s，规则数=%u，输入=%u 字节\n",
           config->algorithm, config->mode, config->rule_count, input_len);
    ret = hash ? run_hash_case(config, input, input_len, &actual) :
                 run_ac_case(config, input, input_len, &actual);
    if (ret != 0) {
        printf("接口调用失败。\n");
        return 1;
    }
    printf("扫描完成：命中ID数量=%u\n", actual);
    return 0;
}

int demo_run_benchmark_case(const demo_benchmark_case_t *config)
{
    demo_corpus_header_t header;
    demo_record_header_t record;
    uint8_t *payload = NULL;
    uint32_t expected;
    uint32_t actual = 0;
    uint32_t expected_kind;
    int hash;
    int ret;

    hash = strcmp(config->algorithm, "hash") == 0;
    expected_kind = hash ? DEMO_KIND_HASH :
                    strcmp(config->algorithm, "accombo") == 0 ||
                    strcmp(config->algorithm, "ac_on_combo") == 0 ?
                    DEMO_KIND_ACCOMBO : DEMO_KIND_AC;
    if ((!hash && strcmp(config->algorithm, "ac") != 0 &&
         strcmp(config->algorithm, "accombo") != 0 &&
         strcmp(config->algorithm, "ac_legacy") != 0 &&
         strcmp(config->algorithm, "ac_on_combo") != 0) ||
        (strcmp(config->mode, "lookup") != 0 &&
         strcmp(config->mode, "block") != 0 &&
         strcmp(config->mode, "stream-full") != 0) ||
        (hash && strcmp(config->mode, "lookup") != 0) ||
        (!hash && strcmp(config->mode, "lookup") == 0)) {
        printf("算法或扫描模式不匹配。\n");
        return 1;
    }
    if (config->all_queries) {
        return run_full_corpus(config);
    }
    if (load_query(config->corpus_file, config->query_index,
                   &header, &record, &payload) != 0) {
        return 1;
    }
    if (header.kind != expected_kind) {
        printf("算法与 corpus 类型不匹配：corpus kind=%u\n", header.kind);
        free(payload);
        return 1;
    }

    printf("920hg 数据场景：算法=%s，模式=%s，规则数=%u，查询序号=%u\n",
           config->algorithm, config->mode, config->rule_count, config->query_index);
    printf("corpus：查询数=%u，payload=%u 字节，期望ID=%u，候选ID=%u\n",
           header.query_count, header.payload_size,
           record.expected_ids, record.expected_candidates);
    ret = hash ? run_hash_case(config, payload, header.payload_size, &actual) :
                 run_ac_case(config, payload, header.payload_size, &actual);
    expected = strcmp(config->algorithm, "ac_on_combo") == 0 ?
               record.expected_candidates : record.expected_ids;
    free(payload);
    if (ret != 0) {
        printf("接口调用失败。\n");
        return 1;
    }
    printf("结果校验：期望ID数量=%u，实际ID数量=%u，状态=%s\n",
           expected, actual, expected == actual ? "通过" : "失败");
    return expected == actual ? 0 : 1;
}

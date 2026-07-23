/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Parameterized CLI calls for most public Megascan APIs.
 */
#include "api_cli_demo.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo_common.h"
#include "ms.h"
#include "ms_allocator.h"
#include "ms_compile.h"
#include "ms_expr.h"
#include "ms_handler.h"
#include "ms_logical.h"
#include "ms_rule.h"
#include "ms_runtime.h"

#define CLI_MAX_OUTPUT_CAPACITY 65536u

typedef struct {
    const char **expressions;
    uint32_t *ids;
    uint32_t *flags;
    ms_expr_ext_t *ext;
    uint32_t count;
} cli_expr_spec_t;

typedef struct {
    ms_rule_t *rules;
    uint32_t count;
} cli_rule_spec_t;

static size_t g_cli_allocs;
static size_t g_cli_callocs;
static size_t g_cli_frees;

static int cli_same(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

static int cli_parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL || text[0] == '-') {
        return -1;
    }
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int cli_parse_matcher(const char *text, ms_matcher_type *type)
{
    if (cli_same(text, "hash") || cli_same(text, "HASH") || cli_same(text, "1")) {
        *type = MS_MATCHER_TYPE_HASH;
        return 0;
    }
    if (cli_same(text, "ac") || cli_same(text, "AC") || cli_same(text, "2")) {
        *type = MS_MATCHER_TYPE_AC;
        return 0;
    }
    return -1;
}

static const char *cli_ret_name(int ret)
{
    switch (ret) {
        case MS_SUCCESS: return "MS_SUCCESS";
        case MS_INVALID_PARA: return "MS_INVALID_PARA";
        case MS_MALLOC_FAIL: return "MS_MALLOC_FAIL";
        case MS_BUILT_ALREADY: return "MS_BUILT_ALREADY";
        case MS_FIND_FAIL: return "MS_FIND_FAIL";
        case MS_INIT_RWLOCK_FAIL: return "MS_INIT_RWLOCK_FAIL";
        case MS_MEMCPY_FAIL: return "MS_MEMCPY_FAIL";
        case MS_NOT_SUPPORTED: return "MS_NOT_SUPPORTED";
        case MS_STREAM_STALE: return "MS_STREAM_STALE";
        default: return "未知返回码";
    }
}

static void cli_print_ret(const char *api, int ret)
{
    printf("%s ret=%d(%s)\n", api, ret, cli_ret_name(ret));
}

static int cli_result(int ret)
{
    return (ret == MS_SUCCESS || ret == MS_FIND_FAIL) ? 0 : 1;
}

static int cli_parse_capacity(const char *text, uint32_t *capacity)
{
    if (cli_parse_u32(text, capacity) != 0 || *capacity > CLI_MAX_OUTPUT_CAPACITY) {
        printf("id_capacity 必须位于 0..%u。\n", CLI_MAX_OUTPUT_CAPACITY);
        return -1;
    }
    return 0;
}

static uint32_t *cli_alloc_output(uint32_t capacity)
{
    return capacity == 0 ? NULL : (uint32_t *)calloc(capacity, sizeof(uint32_t));
}

static void cli_print_output(const uint32_t *ids, uint32_t count, uint32_t capacity)
{
    uint32_t printable = count < capacity ? count : capacity;

    printf("count=%u", count);
    if (count > capacity) {
        printf(" required_capacity=%u", count);
    }
    printf(" ids=");
    for (uint32_t i = 0; i < printable; i++) {
        printf("%s%u", i == 0 ? "" : ",", ids[i]);
    }
    printf("\n");
}

static void cli_free_expr_spec(cli_expr_spec_t *spec)
{
    if (spec == NULL) return;
    free((void *)spec->expressions);
    free(spec->ids);
    free(spec->flags);
    free(spec->ext);
    (void)memset(spec, 0, sizeof(*spec));
}

/*
 * Serialize the C signature without interleaving arrays:
 *   expressions[0..N), flags[0..N), ids[0..N), elements
 */
static int cli_parse_expr_spec_ordered(int argc, char **argv,
                                       cli_expr_spec_t *spec,
                                       int ids_before_flags)
{
    uint32_t array_count;
    uint32_t elements;
    uint32_t i;

    if (argc < 4 || ((uint32_t)(argc - 1) % 3u) != 0) {
        printf("参数错误：表达式参数必须严格按 expressions[N] flags[N] "
               "ids[N] elements 排列。\n");
        return -1;
    }
    array_count = (uint32_t)(argc - 1) / 3u;
    if (cli_parse_u32(argv[argc - 1], &elements) != 0) {
        printf("参数错误：elements 必须是 uint32_t。\n");
        return -1;
    }
    if (elements != array_count) {
        printf("参数错误：序列化了 %u 条 expression/flag/id，但 elements=%u；"
               "两者必须完全相等。\n", array_count, elements);
        return -1;
    }
    spec->count = elements;
    spec->expressions = (const char **)calloc(array_count + 1u,
                                              sizeof(*spec->expressions));
    spec->ids = (uint32_t *)calloc(array_count, sizeof(*spec->ids));
    spec->flags = (uint32_t *)calloc(array_count, sizeof(*spec->flags));
    if (spec->expressions == NULL || spec->ids == NULL || spec->flags == NULL) {
        cli_free_expr_spec(spec);
        return -1;
    }
    for (i = 0; i < array_count; i++) {
        const char *flag_text = argv[array_count + i];
        const char *id_text = argv[array_count * 2u + i];

        spec->expressions[i] = argv[i];
        if (ids_before_flags) {
            const char *swap = flag_text;
            flag_text = id_text;
            id_text = swap;
        }
        if (cli_parse_u32(flag_text, &spec->flags[i]) != 0 ||
            cli_parse_u32(id_text, &spec->ids[i]) != 0) {
            cli_free_expr_spec(spec);
            return -1;
        }
    }
    return 0;
}

static int cli_parse_u64(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || value == NULL || text[0] == '-') return -1;
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return -1;
    *value = (uint64_t)parsed;
    return 0;
}

static int cli_parse_expr_spec(int argc, char **argv, cli_expr_spec_t *spec)
{
    return cli_parse_expr_spec_ordered(argc, argv, spec, 0);
}

/*
 * expressions[0..N), flags[0..N), ids[0..N),
 * ext[0].flags/min/max ... ext[N-1].flags/min/max, elements
 */
static int cli_parse_expr_spec_ext(int argc, char **argv, cli_expr_spec_t *spec)
{
    uint32_t array_count;
    uint32_t elements;
    uint32_t i;

    if (argc < 7 || ((uint32_t)(argc - 1) % 6u) != 0) {
        printf("参数错误：扩展表达式参数必须严格按 expressions[N] flags[N] "
               "ids[N] ext[N] elements 排列，每项 ext 为 flags/min/max。\n");
        return -1;
    }
    array_count = (uint32_t)(argc - 1) / 6u;
    if (cli_parse_u32(argv[argc - 1], &elements) != 0) {
        printf("参数错误：elements 必须是 uint32_t。\n");
        return -1;
    }
    if (elements != array_count) {
        printf("参数错误：序列化了 %u 条 expression/flag/id/ext，但 "
               "elements=%u；两者必须完全相等。\n",
               array_count, elements);
        return -1;
    }
    spec->count = elements;
    spec->expressions = (const char **)calloc(array_count + 1u,
                                              sizeof(*spec->expressions));
    spec->ids = (uint32_t *)calloc(array_count, sizeof(*spec->ids));
    spec->flags = (uint32_t *)calloc(array_count, sizeof(*spec->flags));
    spec->ext = (ms_expr_ext_t *)calloc(array_count, sizeof(*spec->ext));
    if (spec->expressions == NULL || spec->ids == NULL ||
        spec->flags == NULL || spec->ext == NULL) {
        cli_free_expr_spec(spec);
        return -1;
    }
    for (i = 0; i < array_count; i++) {
        uint32_t ext_offset = array_count * 3u + i * 3u;
        spec->expressions[i] = argv[i];
        if (cli_parse_u32(argv[array_count + i], &spec->flags[i]) != 0 ||
            cli_parse_u32(argv[array_count * 2u + i], &spec->ids[i]) != 0 ||
            cli_parse_u32(argv[ext_offset], &spec->ext[i].flags) != 0 ||
            cli_parse_u64(argv[ext_offset + 1u], &spec->ext[i].min_offset) != 0 ||
            cli_parse_u64(argv[ext_offset + 2u], &spec->ext[i].max_offset) != 0) {
            cli_free_expr_spec(spec);
            return -1;
        }
    }
    return 0;
}

static int cli_parse_db_prefix(int argc, char **argv, ms_matcher_type *type,
                               uint32_t *buckets, uint32_t *secondary)
{
    return argc >= 3 && cli_parse_matcher(argv[0], type) == 0 &&
           cli_parse_u32(argv[1], buckets) == 0 &&
           cli_parse_u32(argv[2], secondary) == 0 ? 0 : -1;
}

static int cli_compile_db(int argc, char **argv, ms_database_t **db)
{
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    cli_expr_spec_t spec = {0};
    int ret;

    if (cli_parse_db_prefix(argc, argv, &type, &buckets, &secondary) != 0 ||
        cli_parse_expr_spec(argc - 3, argv + 3, &spec) != 0) {
        return MS_INVALID_PARA;
    }
    ret = ms_compile_database_multi(db, type, buckets, secondary,
                                    spec.expressions, spec.flags, spec.ids,
                                    spec.count);
    cli_free_expr_spec(&spec);
    return ret;
}

static int cli_compile_handler(int argc, char **argv, ms_handler_t *handler,
                               int extended, int add_exprs)
{
    cli_expr_spec_t spec = {0};
    int ret;

    ret = ms_handler_init(handler, MS_MATCHER_TYPE_AC, 0, 0);
    if (ret != MS_SUCCESS) return ret;
    if ((extended ? cli_parse_expr_spec_ext(argc, argv, &spec) :
         add_exprs ? cli_parse_expr_spec_ordered(argc, argv, &spec, 1) :
                     cli_parse_expr_spec(argc, argv, &spec)) != 0) {
        (void)ms_handler_destroy(handler);
        (void)memset(handler, 0, sizeof(*handler));
        return MS_INVALID_PARA;
    }
    ret = extended ?
          ms_compile_multi_ext(handler, spec.expressions, spec.flags, spec.ids,
                               spec.ext, spec.count) :
          add_exprs ?
          ms_add_exprs(handler, spec.expressions, spec.ids, spec.flags,
                       spec.count) :
          ms_compile_multi(handler, spec.expressions, spec.flags, spec.ids,
                           spec.count);
    cli_free_expr_spec(&spec);
    if (ret != MS_SUCCESS) {
        (void)ms_handler_destroy(handler);
        (void)memset(handler, 0, sizeof(*handler));
    }
    return ret;
}

static int cli_parse_rules(int argc, char **argv, cli_rule_spec_t *spec)
{
    uint32_t i;

    if (argc <= 0 || argc % 3 != 0) return -1;
    spec->count = (uint32_t)argc / 3u;
    spec->rules = (ms_rule_t *)calloc(spec->count, sizeof(*spec->rules));
    if (spec->rules == NULL) return -1;
    for (i = 0; i < spec->count; i++) {
        const char *key = argv[i * 3u + 2u];
        if (cli_parse_u32(argv[i * 3u], &spec->rules[i].alias) != 0 ||
            cli_parse_u32(argv[i * 3u + 1u], &spec->rules[i].key_len) != 0 ||
            (!cli_same(key, "-") &&
             spec->rules[i].key_len > strlen(key))) {
            free(spec->rules);
            (void)memset(spec, 0, sizeof(*spec));
            return -1;
        }
        spec->rules[i].key = cli_same(key, "-") ? NULL : (uint8_t *)key;
    }
    return 0;
}

static uint32_t cli_hash(const uint8_t *key, uint32_t key_len)
{
    uint32_t hash = 2166136261u;
    uint32_t i;
    for (i = 0; i < key_len; i++) {
        hash ^= key[i];
        hash *= 16777619u;
    }
    return hash;
}

static void *cli_alloc(size_t size)
{
    g_cli_allocs++;
    return malloc(size);
}

static void *cli_calloc(size_t nmemb, size_t size)
{
    g_cli_callocs++;
    return calloc(nmemb, size);
}

static void cli_free(void *ptr)
{
    g_cli_frees++;
    free(ptr);
}

static int cli_parse_assignments(uint8_t *vector, uint32_t logical_count,
                                 const char *text)
{
    const char *cursor = text;

    if (text == NULL || text[0] == '\0' || cli_same(text, "-")) return MS_SUCCESS;
    while (*cursor != '\0') {
        char *end = NULL;
        unsigned long key = strtoul(cursor, &end, 10);
        uint8_t value;
        int ret;

        if (end == cursor || key > UINT32_MAX || *end != '=' ||
            (end[1] != '0' && end[1] != '1')) {
            return MS_INVALID_PARA;
        }
        value = (uint8_t)(end[1] - '0');
        ret = ms_logical_set(vector, logical_count, (uint32_t)key, value);
        if (ret != MS_SUCCESS) return ret;
        cursor = end + 2;
        if (*cursor == ',') cursor++;
        else if (*cursor != '\0') return MS_INVALID_PARA;
    }
    return MS_SUCCESS;
}

static int cli_parse_aliases(const char *text, uint32_t **aliases,
                             uint32_t *count)
{
    const char *cursor = text;
    uint32_t capacity = 4;

    *aliases = (uint32_t *)calloc(capacity, sizeof(**aliases));
    if (*aliases == NULL || text == NULL || text[0] == '\0') {
        free(*aliases);
        return -1;
    }
    while (*cursor != '\0') {
        char *end = NULL;
        unsigned long parsed;
        uint32_t value;

        errno = 0;
        parsed = strtoul(cursor, &end, 10);
        if (errno != 0 || end == cursor || parsed > UINT32_MAX ||
            (*end != ',' && *end != '\0')) {
            free(*aliases);
            return -1;
        }
        value = (uint32_t)parsed;
        if (*count == capacity) {
            uint32_t *grown;
            capacity *= 2u;
            grown = (uint32_t *)realloc(*aliases, capacity * sizeof(**aliases));
            if (grown == NULL) {
                free(*aliases);
                return -1;
            }
            *aliases = grown;
        }
        (*aliases)[(*count)++] = value;
        cursor = *end == ',' ? end + 1 : end;
    }
    return *count == 0 ? -1 : 0;
}

static int cli_database_lifecycle(const char *api, int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    int ret;
    int cleanup_ret = MS_SUCCESS;

    if (argc != 3 ||
        cli_parse_db_prefix(argc, argv, &type, &buckets, &secondary) != 0) {
        printf("用法：--api %s <hash|ac> <buckets> <secondary_buckets>\n", api);
        return 1;
    }
    ret = ms_create_database(&db, type, buckets, secondary);
    if (cli_same(api, "ms_create_database")) {
        cli_print_ret(api, ret);
        printf("output.db=%s\n", db == NULL ? "NULL" : "CREATED");
        if (db != NULL) cleanup_ret = ms_free_database(db);
        printf("cleanup.ms_free_database ret=%d(%s)\n",
               cleanup_ret, cli_ret_name(cleanup_ret));
        return ret == MS_SUCCESS && db != NULL &&
               cleanup_ret == MS_SUCCESS ? 0 : 1;
    } else if (ret == MS_SUCCESS) {
        printf("fixture.db=CREATED\n");
        ret = ms_free_database(db);
        db = NULL;
        cli_print_ret(api, ret);
        printf("output.db=%s\n", ret == MS_SUCCESS ? "RELEASED" : "NOT_RELEASED");
    }
    if (db != NULL) (void)ms_free_database(db);
    return cli_result(ret);
}

static int cli_compile_database_ext(int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    cli_expr_spec_t spec = {0};
    int ret;
    int cleanup_ret = MS_SUCCESS;

    if (cli_parse_db_prefix(argc, argv, &type, &buckets, &secondary) != 0 ||
        cli_parse_expr_spec_ext(argc - 3, argv + 3, &spec) != 0) {
        printf("用法：--api ms_compile_database_multi_ext <hash|ac> <buckets> <secondary> "
               "<expressions...> <flags...> <ids...> "
               "<ext_flags min max>... <elements>\n");
        printf("ms_compile_database_multi_ext called=NO\n");
        return 1;
    }
    ret = ms_compile_database_multi_ext(&db, type, buckets, secondary,
                                        spec.expressions, spec.flags, spec.ids,
                                        spec.ext, spec.count);
    cli_print_ret("ms_compile_database_multi_ext", ret);
    printf("output.db=%s\n", db == NULL ? "NULL" : "CREATED");
    if (db != NULL) cleanup_ret = ms_free_database(db);
    printf("cleanup.ms_free_database ret=%d(%s)\n",
           cleanup_ret, cli_ret_name(cleanup_ret));
    cli_free_expr_spec(&spec);
    return ret == MS_SUCCESS && db != NULL &&
           cleanup_ret == MS_SUCCESS ? 0 : 1;
}

static int cli_find_database_rule(int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_rule_t query;
    int ret;

    if (argc < 10 || cli_parse_u32(argv[0], &query.alias) != 0 ||
        cli_parse_u32(argv[1], &query.key_len) != 0 ||
        query.key_len > strlen(argv[2])) {
        printf("用法：--api ms_find_database_rule <alias> <key_len> <key> "
               "<hash|ac> <buckets> "
               "<secondary> <expressions...> <flags...> <ids...> <elements>\n");
        return 1;
    }
    ret = cli_compile_db(argc - 3, argv + 3, &db);
    if (ret != MS_SUCCESS) {
        printf("PRECONDITION api=ms_find_database_rule source=AUTO "
               "objects=database status=FAILED reason=STANDALONE_CALL\n");
        cli_print_ret("SETUP", ret);
        printf("ms_find_database_rule called=NO\n");
        return 1;
    }
    printf("PRECONDITION api=ms_find_database_rule source=AUTO "
           "objects=database status=READY reason=STANDALONE_CALL\n");
    query.key = (uint8_t *)argv[2];
    ret = ms_find_database_rule(db, &query);
    cli_print_ret("ms_find_database_rule", ret);
    printf("alias=%u\n", query.alias);
    {
        int cleanup_ret = ms_free_database(db);
        printf("CLEANUP source=AUTO ret=%d(%s)\n",
               cleanup_ret, cli_ret_name(cleanup_ret));
    }
    return cli_result(ret);
}

static int cli_database_stream_api(const char *api, int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t capacity = 0;
    uint32_t data_len = 0;
    uint32_t *ids = NULL;
    uint32_t count = 0;
    const char *data = NULL;
    int spec_offset = 0;
    int ret;
    int cleanup_ret = MS_SUCCESS;

    if (cli_same(api, "ms_scan_stream")) {
        if (argc < 10 || cli_parse_u32(argv[1], &data_len) != 0 ||
            data_len > strlen(argv[0]) ||
            cli_parse_capacity(argv[2], &capacity) != 0) {
            printf("用法：--api ms_scan_stream <data> <len> <id_capacity> "
                   "<hash|ac> <buckets> <secondary> "
                   "<expressions...> <flags...> <ids...> <elements>\n");
            return 1;
        }
        data = argv[0];
        spec_offset = 3;
    } else if (cli_same(api, "ms_flush_stream") ||
               cli_same(api, "ms_close_stream")) {
        if (argc < 8 || cli_parse_capacity(argv[0], &capacity) != 0) {
            printf("用法：--api %s <id_capacity> <hash|ac> <buckets> "
                   "<secondary> <expressions...> <flags...> <ids...> "
                   "<elements>\n", api);
            return 1;
        }
        spec_offset = 1;
    } else {
        if (argc < 7) {
            printf("用法：--api ms_reset_stream <hash|ac> <buckets> <secondary> "
                   "<expressions...> <flags...> <ids...> <elements>\n");
            return 1;
        }
    }

    ret = cli_compile_db(argc - spec_offset, argv + spec_offset, &db);
    if (ret == MS_SUCCESS) ret = ms_alloc_scratch(db, &scratch);
    if (ret == MS_SUCCESS) ret = ms_open_stream(db, &stream);
    if (ret != MS_SUCCESS) {
        printf("PRECONDITION api=%s source=AUTO "
               "objects=database,scratch,stream status=FAILED "
               "reason=STANDALONE_CALL\n", api);
        cli_print_ret("SETUP", ret);
        printf("%s called=NO\n", api);
        goto cleanup;
    }
    printf("PRECONDITION api=%s source=AUTO "
           "objects=database,scratch,stream status=READY "
           "reason=STANDALONE_CALL\n", api);
    ids = cli_alloc_output(capacity);
    if (capacity != 0 && ids == NULL) {
        ret = MS_MALLOC_FAIL;
        cli_print_ret("OUTPUT_SETUP", ret);
        printf("%s called=NO\n", api);
        goto cleanup;
    }
    if (cli_same(api, "ms_scan_stream")) {
        ret = ms_scan_stream(stream, (const uint8_t *)data, data_len,
                             scratch, ids, &count, capacity);
    } else if (cli_same(api, "ms_flush_stream")) {
        count = 0;
        ret = ms_flush_stream(stream, scratch, ids, &count, capacity);
    } else if (cli_same(api, "ms_close_stream")) {
        count = 0;
        ret = ms_close_stream(stream, scratch, ids, &count, capacity);
        stream = NULL;
    } else if (cli_same(api, "ms_reset_stream")) {
        ret = ms_reset_stream(stream);
        cli_print_ret(api, ret);
    }
    if (!cli_same(api, "ms_reset_stream")) {
        cli_print_ret(api, ret);
        cli_print_output(ids, count, capacity);
    }

cleanup:
    if (stream != NULL) {
        uint32_t ignored = 0;
        cleanup_ret = ms_close_stream(stream, scratch, ids, &ignored, capacity);
    }
    free(ids);
    if (scratch != NULL) {
        int free_ret = ms_free_scratch(scratch);
        if (cleanup_ret == MS_SUCCESS) cleanup_ret = free_ret;
    }
    if (db != NULL) {
        int free_ret = ms_free_database(db);
        if (cleanup_ret == MS_SUCCESS) cleanup_ret = free_ret;
    }
    printf("CLEANUP source=AUTO ret=%d(%s)\n",
           cleanup_ret, cli_ret_name(cleanup_ret));
    return cli_result(ret) == 0 && cleanup_ret == MS_SUCCESS ? 0 : 1;
}

static int cli_handler_lifecycle(const char *api, int argc, char **argv)
{
    ms_handler_t handler = {0};
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    int ret;

    if (argc != (cli_same(api, "ms_set_hash_func") ? 4 : 3) ||
        cli_parse_db_prefix(argc, argv, &type, &buckets, &secondary) != 0) {
        printf("用法：--api %s <hash|ac> <buckets> <secondary_buckets>%s\n",
               api, cli_same(api, "ms_set_hash_func") ?
               " <custom|null>" : "");
        return 1;
    }
    ret = ms_handler_init(&handler, type, buckets, secondary);
    if (cli_same(api, "ms_handler_init")) {
        cli_print_ret(api, ret);
    } else if (ret == MS_SUCCESS && cli_same(api, "ms_set_hash_func")) {
        if (!cli_same(argv[3], "custom") && !cli_same(argv[3], "null")) {
            ret = MS_INVALID_PARA;
        } else {
            ret = ms_set_hash_func(&handler,
                                   cli_same(argv[3], "custom") ?
                                   cli_hash : NULL);
        }
        cli_print_ret(api, ret);
    } else if (ret == MS_SUCCESS) {
        ret = ms_handler_destroy(&handler);
        (void)memset(&handler, 0, sizeof(handler));
        cli_print_ret(api, ret);
    }
    if (handler.ms_matcher != NULL) (void)ms_handler_destroy(&handler);
    return cli_result(ret);
}

static int cli_handler_rule_api(const char *api, int argc, char **argv)
{
    ms_handler_t handler = {0};
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    uint32_t rule_num = 0;
    cli_rule_spec_t spec = {0};
    ms_rule_t target = {0};
    int prefix = cli_same(api, "ms_add_rule") ? 0 :
                 cli_same(api, "ms_find_rule") ? 3 : 4;
    int ret;

    if (argc < prefix + 5 ||
        cli_parse_db_prefix(argc - prefix, argv + prefix,
                            &type, &buckets, &secondary) != 0 ||
        cli_parse_rules(argc - prefix - 3 -
                        (cli_same(api, "ms_add_rule") ? 1 : 0),
                        argv + prefix + 3, &spec) != 0 ||
        (cli_same(api, "ms_add_rule") &&
         (cli_parse_u32(argv[argc - 1], &rule_num) != 0 ||
          rule_num != spec.count))) {
        printf("用法：--api ms_add_rule <hash|ac> <buckets> <secondary> "
               "<alias key_len key>... <rule_num>\n");
        printf("      --api ms_find_rule <alias> <key_len> <key> "
               "<hash|ac> <buckets> "
               "<secondary> <alias key_len key>...\n");
        printf("      --api ms_remove_rule <alias> <key_len> <key|-> <rule_num> "
               "<hash|ac> <buckets> <secondary> <alias key_len key>...\n");
        return 1;
    }
    ret = ms_handler_init(&handler, type, buckets, secondary);
    if (ret == MS_SUCCESS) {
        ret = ms_add_rule(&handler, spec.rules,
                          cli_same(api, "ms_add_rule") ?
                          rule_num : spec.count);
    }
    if (ret != MS_SUCCESS || cli_same(api, "ms_add_rule")) {
        cli_print_ret(api, ret);
        goto cleanup;
    }
    if (!cli_same(api, "ms_add_rule") &&
        (cli_parse_u32(argv[0], &target.alias) != 0 ||
         cli_parse_u32(argv[1], &target.key_len) != 0 ||
         (!cli_same(argv[2], "-") && target.key_len > strlen(argv[2])))) {
        ret = MS_INVALID_PARA;
        goto cleanup;
    }
    if (cli_same(api, "ms_remove_rule") &&
        (cli_parse_u32(argv[3], &rule_num) != 0 || rule_num != 1)) {
        ret = MS_INVALID_PARA;
        goto cleanup;
    }
    target.key = cli_same(argv[2], "-") ? NULL : (uint8_t *)argv[2];
    if (cli_same(api, "ms_find_rule")) {
        ret = ms_find_rule(&handler, &target);
        cli_print_ret(api, ret);
        printf("alias=%u\n", target.alias);
    } else {
        ret = ms_remove_rule(&handler, &target, rule_num);
        cli_print_ret(api, ret);
    }

cleanup:
    free(spec.rules);
    if (handler.ms_matcher != NULL) (void)ms_handler_destroy(&handler);
    return cli_same(api, "ms_remove_rule") ? (ret == MS_SUCCESS ? 0 : 1) :
           cli_result(ret);
}

static int cli_compile_handler_target(const char *api, int argc, char **argv)
{
    ms_handler_t handler = {0};
    int extended = cli_same(api, "ms_compile_multi_ext");
    int add_exprs = cli_same(api, "ms_add_exprs");
    cli_expr_spec_t spec = {0};
    int ret;

    ret = ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0);
    if (ret != MS_SUCCESS) return 1;
    if ((extended ? cli_parse_expr_spec_ext(argc, argv, &spec) :
         add_exprs ? cli_parse_expr_spec_ordered(argc, argv, &spec, 1) :
                     cli_parse_expr_spec(argc, argv, &spec)) != 0) {
        printf("用法：--api %s %s\n", api,
               extended ?
               "<expressions...> <flags...> <ids...> "
               "<ext_flags min max>... <elements>" :
               add_exprs ?
               "<expressions...> <ids...> <flags...> <elements>" :
               "<expressions...> <flags...> <ids...> <elements>");
        (void)ms_handler_destroy(&handler);
        return 1;
    }
    ret = extended ?
          ms_compile_multi_ext(&handler, spec.expressions, spec.flags, spec.ids,
                               spec.ext, spec.count) :
          add_exprs ?
          ms_add_exprs(&handler, spec.expressions, spec.ids, spec.flags,
                       spec.count) :
          ms_compile_multi(&handler, spec.expressions, spec.flags, spec.ids,
                           spec.count);
    cli_print_ret(api, ret);
    cli_free_expr_spec(&spec);
    (void)ms_handler_destroy(&handler);
    return cli_result(ret);
}

static int cli_handler_scan_api(const char *api, int argc, char **argv)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    uint32_t *ids = NULL;
    uint32_t *aliases = NULL;
    uint32_t alias_count = 0;
    uint32_t supplied_alias_count = 0;
    uint32_t data_len = 0;
    uint32_t capacity = 0;
    uint32_t count = 0;
    int spec_offset;
    int ret;

    if (cli_same(api, "ms_open_handler_stream") ||
        cli_same(api, "ms_close_handler_stream")) {
        spec_offset = 0;
    } else if (cli_same(api, "ms_scan_rule_stream")) {
        if (argc < 6 || cli_parse_u32(argv[1], &data_len) != 0 ||
            data_len > strlen(argv[0])) {
            printf("用法：--api ms_scan_rule_stream <data> <len> "
                   "<expressions...> <flags...> <ids...> <elements>\n");
            return 1;
        }
        spec_offset = 2;
    } else if (cli_same(api, "ms_scan_expr_stream_flush")) {
        if (argc < 4 || cli_parse_capacity(argv[0], &capacity) != 0) {
            printf("用法：--api ms_scan_expr_stream_flush <id_capacity> "
                   "<expressions...> <flags...> <ids...> <elements>\n");
            return 1;
        }
        spec_offset = 1;
    } else {
        if (argc < 7 ||
            cli_parse_u32(argv[1],
                          cli_same(api, "ms_compile_eval_combinations") ?
                          &supplied_alias_count : &data_len) != 0 ||
            (!cli_same(api, "ms_compile_eval_combinations") &&
             data_len > strlen(argv[0])) ||
            cli_parse_capacity(argv[2], &capacity) != 0) {
            printf("用法：--api %s <%s> <%s> <id_capacity> "
                   "<expressions...> <flags...> <ids...> <elements>\n",
                   api,
                   cli_same(api, "ms_compile_eval_combinations") ?
                   "alias_csv" : "data",
                   cli_same(api, "ms_compile_eval_combinations") ?
                   "alias_count" : "len");
            return 1;
        }
        spec_offset = 3;
    }
    ret = cli_compile_handler(argc - spec_offset, argv + spec_offset,
                              &handler, 0, 0);
    if (ret != MS_SUCCESS) {
        cli_print_ret("SETUP", ret);
        return 1;
    }

    if (cli_same(api, "ms_scan_expr")) {
        ids = cli_alloc_output(capacity);
        ret = capacity != 0 && ids == NULL ? MS_MALLOC_FAIL :
              ms_scan_expr(&handler, (const uint8_t *)argv[0],
                           data_len, ids, &count, capacity);
    } else if (cli_same(api, "ms_compile_eval_combinations")) {
        ids = cli_alloc_output(capacity);
        if ((capacity != 0 && ids == NULL) ||
            cli_parse_aliases(argv[0], &aliases, &alias_count) != 0 ||
            supplied_alias_count != alias_count) {
            if (supplied_alias_count != alias_count) {
                printf("参数错误：alias_csv 包含 %u 项，但 alias_count=%u；"
                       "两者必须完全相等。\n",
                       alias_count, supplied_alias_count);
            }
            ret = MS_INVALID_PARA;
        } else {
            ret = ms_compile_eval_combinations(&handler, aliases,
                                               supplied_alias_count,
                                               ids, &count, capacity);
        }
    } else if (cli_same(api, "ms_compile_scan_ac_combinations")) {
        ids = cli_alloc_output(capacity);
        ret = capacity != 0 && ids == NULL ? MS_MALLOC_FAIL :
              ms_compile_scan_ac_combinations(
                  &handler, (const uint8_t *)argv[0],
                  data_len, ids, &count, capacity);
    } else {
        ret = ms_open_handler_stream(&handler, &stream);
        if (ret != MS_SUCCESS) goto done;
        if (cli_same(api, "ms_open_handler_stream")) {
            /* The target call is the setup call above. */
        } else if (cli_same(api, "ms_scan_expr_stream")) {
            ids = cli_alloc_output(capacity);
            ret = capacity != 0 && ids == NULL ? MS_MALLOC_FAIL :
                  ms_scan_expr_stream(stream, (const uint8_t *)argv[0],
                                      data_len, ids, &count,
                                      capacity);
        } else if (cli_same(api, "ms_scan_rule_stream")) {
            ret = ms_scan_rule_stream(stream, (const uint8_t *)argv[0],
                                      data_len);
        } else if (cli_same(api, "ms_scan_expr_stream_flush")) {
            ids = cli_alloc_output(capacity);
            ret = capacity != 0 && ids == NULL ? MS_MALLOC_FAIL :
                  ms_scan_expr_stream_flush(stream, ids, &count, capacity);
        } else if (cli_same(api, "ms_reset_stream")) {
            ret = ms_reset_stream(stream);
        } else {
            ret = ms_close_handler_stream(stream);
            stream = NULL;
        }
    }

done:
    cli_print_ret(api, ret);
    if (ids != NULL || capacity != 0) cli_print_output(ids, count, capacity);
    if (stream != NULL) (void)ms_close_handler_stream(stream);
    free(ids);
    free(aliases);
    (void)ms_handler_destroy(&handler);
    return cli_result(ret);
}

static int cli_logical_api(const char *api, int argc, char **argv)
{
    ms_logical_tree_t tree = {0};
    uint8_t *vector = NULL;
    uint32_t logical_count = 0;
    uint32_t key = 0;
    uint32_t raw_value = 0;
    uint8_t value = 0;
    uint8_t matched = 0;
    int ret = MS_INVALID_PARA;

    if (cli_same(api, "ms_logical_parse") ||
        cli_same(api, "ms_logical_tree_destroy")) {
        if (argc != 1) {
            printf("用法：--api %s <expression>\n", api);
            return 1;
        }
        ret = ms_logical_parse(argv[0], &tree);
        if (ret == MS_SUCCESS && cli_same(api, "ms_logical_tree_destroy")) {
            ms_logical_tree_destroy(&tree);
            ret = MS_SUCCESS;
        }
        cli_print_ret(api, ret);
        if (tree.ops != NULL) ms_logical_tree_destroy(&tree);
        return cli_result(ret);
    }

    if (cli_same(api, "ms_logical_set") || cli_same(api, "ms_logical_get")) {
        int is_set = cli_same(api, "ms_logical_set");

        if (argc != (is_set ? 4 : 3) ||
            cli_parse_u32(argv[1], &logical_count) != 0 ||
            cli_parse_u32(argv[2], &key) != 0 ||
            (is_set &&
             (cli_parse_u32(argv[3], &raw_value) != 0 || raw_value > 1u))) {
            printf("用法：--api %s <logical_vector_assignments|-> "
                   "<logical_count> <key>%s\n", api, is_set ? " <0|1>" : "");
            return 1;
        }
        vector = (uint8_t *)calloc((logical_count + 7u) / 8u, 1);
        if (vector == NULL) return 1;
        ret = cli_parse_assignments(vector, logical_count, argv[0]);
        if (is_set) {
            if (ret == MS_SUCCESS) {
                ret = ms_logical_set(vector, logical_count, key,
                                     (uint8_t)raw_value);
            }
        } else {
            if (ret == MS_SUCCESS) {
                ret = ms_logical_get(vector, logical_count, key, &value);
            }
            printf("value=%u\n", value);
        }
        cli_print_ret(api, ret);
        free(vector);
        return cli_result(ret);
    }

    if (cli_same(api, "ms_logical_clear")) {
        if (argc != 2 || cli_parse_u32(argv[1], &logical_count) != 0) {
            printf("用法：--api ms_logical_clear "
                   "<logical_vector_assignments|-> <logical_count>\n");
            return 1;
        }
        vector = (uint8_t *)calloc((logical_count + 7u) / 8u, 1);
        if (vector == NULL) return 1;
        ret = cli_parse_assignments(vector, logical_count, argv[0]);
        if (ret == MS_SUCCESS) ret = ms_logical_clear(vector, logical_count);
        cli_print_ret(api, ret);
        free(vector);
        return cli_result(ret);
    }

    if (cli_same(api, "ms_logical_eval")) {
        if (argc != 3 || cli_parse_u32(argv[2], &logical_count) != 0) {
            printf("用法：--api ms_logical_eval <expression> "
                   "<logical_vector_assignments|-> <logical_count>\n");
            return 1;
        }
        vector = (uint8_t *)calloc((logical_count + 7u) / 8u, 1);
        if (vector == NULL) return 1;
        ret = ms_logical_parse(argv[0], &tree);
        if (ret == MS_SUCCESS) ret = cli_parse_assignments(vector, logical_count, argv[1]);
        if (ret == MS_SUCCESS) ret = ms_logical_eval(&tree, vector, logical_count, &matched);
        cli_print_ret(api, ret);
        printf("matched=%u\n", matched);
        if (tree.ops != NULL) ms_logical_tree_destroy(&tree);
        free(vector);
        return cli_result(ret);
    }

    if (cli_same(api, "ms_logical_eval_inplace")) {
        ms_logical_op_t op;
        ms_logical_tree_t direct;
        uint32_t operation;

        if (argc != 6 || cli_parse_u32(argv[5], &logical_count) != 0 ||
            cli_parse_u32(argv[0], &op.id) != 0 ||
            cli_parse_u32(argv[2], &op.left) != 0 ||
            cli_parse_u32(argv[3], &op.right) != 0) {
            printf("用法：--api ms_logical_eval_inplace <result_id> "
                   "<not|and|or> <left> <right> <assignments> "
                   "<logical_count>\n");
            return 1;
        }
        operation = cli_same(argv[1], "not") ? MS_LOGICAL_OP_NOT :
                    cli_same(argv[1], "and") ? MS_LOGICAL_OP_AND :
                    cli_same(argv[1], "or") ? MS_LOGICAL_OP_OR : UINT32_MAX;
        if (operation == UINT32_MAX) return 1;
        op.op = operation;
        direct.ops = &op;
        direct.op_count = 1;
        direct.op_capacity = 1;
        direct.result = op.id;
        vector = (uint8_t *)calloc((logical_count + 7u) / 8u, 1);
        if (vector == NULL) return 1;
        ret = cli_parse_assignments(vector, logical_count, argv[4]);
        if (ret == MS_SUCCESS) {
            ret = ms_logical_eval_inplace(&direct, vector, logical_count, &matched);
        }
        cli_print_ret(api, ret);
        printf("matched=%u\n", matched);
        free(vector);
        return cli_result(ret);
    }
    return -1;
}

static int cli_allocator_api(const char *api, int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t ids[1] = {0};
    uint32_t count = 0;
    int custom_alloc;
    int custom_calloc;
    int custom_free;
    int any_custom;
    int ret;
    int exercise_ret = MS_SUCCESS;
    int routed = 1;

    if (argc != 3 ||
        (!cli_same(argv[0], "custom") && !cli_same(argv[0], "default")) ||
        (!cli_same(argv[1], "custom") && !cli_same(argv[1], "default")) ||
        (!cli_same(argv[2], "custom") && !cli_same(argv[2], "default"))) {
        printf("用法：--api %s <alloc:custom|default> "
               "<calloc:custom|default> <free:custom|default>\n", api);
        return 1;
    }
    custom_alloc = cli_same(argv[0], "custom");
    custom_calloc = cli_same(argv[1], "custom");
    custom_free = cli_same(argv[2], "custom");
    any_custom = custom_alloc || custom_calloc || custom_free;
    g_cli_allocs = g_cli_callocs = g_cli_frees = 0;
    if (cli_same(api, "ms_set_allocator")) {
        ret = ms_set_allocator(custom_alloc ? cli_alloc : NULL,
                               custom_calloc ? cli_calloc : NULL,
                               custom_free ? cli_free : NULL);
    } else if (cli_same(api, "ms_set_database_allocator")) {
        ret = ms_set_database_allocator(custom_alloc ? cli_alloc : NULL,
                                        custom_calloc ? cli_calloc : NULL,
                                        custom_free ? cli_free : NULL);
    } else if (cli_same(api, "ms_set_scratch_allocator")) {
        ret = ms_set_scratch_allocator(custom_alloc ? cli_alloc : NULL,
                                       custom_calloc ? cli_calloc : NULL,
                                       custom_free ? cli_free : NULL);
    } else if (cli_same(api, "ms_set_stream_allocator")) {
        ret = ms_set_stream_allocator(custom_alloc ? cli_alloc : NULL,
                                      custom_calloc ? cli_calloc : NULL,
                                      custom_free ? cli_free : NULL);
    } else if (cli_same(api, "ms_set_misc_allocator")) {
        ret = ms_set_misc_allocator(custom_alloc ? cli_alloc : NULL,
                                    custom_calloc ? cli_calloc : NULL,
                                    custom_free ? cli_free : NULL);
    } else {
        return -1;
    }
    cli_print_ret(api, ret);
    if (ret != MS_SUCCESS) return cli_result(ret);

    /*
     * A setter-only call does not prove allocator routing. Exercise the object
     * category named by the API while the callback is installed.
     */
    if (any_custom && !cli_same(api, "ms_set_misc_allocator")) {
        exercise_ret = ms_compile_database_multi(
            &db, MS_MATCHER_TYPE_AC, 0, 0,
            (const char *const[]){"abc"}, NULL, (const uint32_t[]){101}, 1);
        if (exercise_ret == MS_SUCCESS &&
            (cli_same(api, "ms_set_allocator") ||
             cli_same(api, "ms_set_scratch_allocator") ||
             cli_same(api, "ms_set_stream_allocator"))) {
            exercise_ret = ms_alloc_scratch(db, &scratch);
        }
        if (exercise_ret == MS_SUCCESS &&
            (cli_same(api, "ms_set_allocator") ||
             cli_same(api, "ms_set_stream_allocator"))) {
            exercise_ret = ms_open_stream(db, &stream);
        }
        if (stream != NULL) {
            (void)ms_close_stream(stream, scratch, ids, &count, 1);
            stream = NULL;
        }
        if (scratch != NULL) {
            (void)ms_free_scratch(scratch);
            scratch = NULL;
        }
        if (db != NULL) {
            (void)ms_free_database(db);
            db = NULL;
        }
        routed = exercise_ret == MS_SUCCESS;
        printf("route=%s exercise=%s\n",
               routed ? "PASS" : "FAIL", cli_ret_name(exercise_ret));
    } else if (any_custom) {
        printf("route=N/A(misc_has_no_public_consumer)\n");
    }
    printf("malloc=%zu calloc=%zu free=%zu\n",
           g_cli_allocs, g_cli_callocs, g_cli_frees);
    /* A CLI invocation must never leave process-global allocators changed. */
    if (any_custom) {
        if (cli_same(api, "ms_set_allocator")) {
            (void)ms_set_allocator(NULL, NULL, NULL);
        } else if (cli_same(api, "ms_set_database_allocator")) {
            (void)ms_set_database_allocator(NULL, NULL, NULL);
        } else if (cli_same(api, "ms_set_scratch_allocator")) {
            (void)ms_set_scratch_allocator(NULL, NULL, NULL);
        } else if (cli_same(api, "ms_set_stream_allocator")) {
            (void)ms_set_stream_allocator(NULL, NULL, NULL);
        } else {
            (void)ms_set_misc_allocator(NULL, NULL, NULL);
        }
    }
    return routed ? cli_result(ret) : 1;
}

int demo_run_api_cli_extended(const char *api, int argc, char **argv)
{
    if (cli_same(api, "ms_create_database") ||
        cli_same(api, "ms_free_database")) {
        return cli_database_lifecycle(api, argc, argv);
    }
    if (cli_same(api, "ms_compile_database_multi_ext")) {
        return cli_compile_database_ext(argc, argv);
    }
    if (cli_same(api, "ms_find_database_rule")) {
        return cli_find_database_rule(argc, argv);
    }
    if (cli_same(api, "ms_scan_stream") ||
        cli_same(api, "ms_flush_stream") ||
        cli_same(api, "ms_close_stream") ||
        cli_same(api, "ms_reset_stream")) {
        return cli_database_stream_api(api, argc, argv);
    }
    if (cli_same(api, "ms_handler_init") ||
        cli_same(api, "ms_handler_destroy") ||
        cli_same(api, "ms_set_hash_func")) {
        return cli_handler_lifecycle(api, argc, argv);
    }
    if (cli_same(api, "ms_add_rule") ||
        cli_same(api, "ms_find_rule") ||
        cli_same(api, "ms_remove_rule")) {
        return cli_handler_rule_api(api, argc, argv);
    }
    if (cli_same(api, "ms_compile_multi") ||
        cli_same(api, "ms_compile_multi_ext") ||
        cli_same(api, "ms_add_exprs")) {
        return cli_compile_handler_target(api, argc, argv);
    }
    if (cli_same(api, "ms_scan_expr") ||
        cli_same(api, "ms_compile_eval_combinations") ||
        cli_same(api, "ms_compile_scan_ac_combinations") ||
        cli_same(api, "ms_open_handler_stream") ||
        cli_same(api, "ms_scan_expr_stream") ||
        cli_same(api, "ms_scan_rule_stream") ||
        cli_same(api, "ms_scan_expr_stream_flush") ||
        cli_same(api, "ms_close_handler_stream")) {
        return cli_handler_scan_api(api, argc, argv);
    }
    if (strncmp(api, "ms_logical_", strlen("ms_logical_")) == 0) {
        return cli_logical_api(api, argc, argv);
    }
    if (strncmp(api, "ms_set_", strlen("ms_set_")) == 0 &&
        strstr(api, "allocator") != NULL) {
        return cli_allocator_api(api, argc, argv);
    }
    return -1;
}

void demo_print_api_cli_extended_index(void)
{
    printf("  ms_create_database            <hash|ac> <buckets> <secondary>\n");
    printf("  ms_free_database              <hash|ac> <buckets> <secondary>\n");
    printf("  ms_compile_database_multi_ext <type> <buckets> <secondary> "
           "<expr[]> <flags[]> <ids[]> <ext[]> <elements>\n");
    printf("  ms_find_database_rule         <alias> <key_len> <key> <db fixture...>\n");
    printf("  ms_scan_stream                <data> <len> <capacity> <db fixture...>\n");
    printf("  ms_flush_stream               <capacity> <db fixture...>\n");
    printf("  ms_close_stream               <capacity> <db fixture...>\n");
    printf("  ms_reset_stream               <db fixture...>\n");
    printf("  ms_handler_init|destroy       <hash|ac> <buckets> <secondary>\n");
    printf("  ms_set_hash_func              <hash> <buckets> <secondary> <custom|null>\n");
    printf("  ms_add_rule                   <matcher fixture> <rule[]> <rule_num>\n");
    printf("  ms_find_rule                  <rule> <matcher fixture> <fixture rules>\n");
    printf("  ms_remove_rule                <rule> <rule_num> <matcher fixture>\n");
    printf("  ms_compile_multi              <expr[]> <flags[]> <ids[]> <elements>\n");
    printf("  ms_add_exprs                  <expr[]> <ids[]> <flags[]> <elements>\n");
    printf("  ms_compile_multi_ext          <expr[]> <flags[]> <ids[]> <ext[]> <elements>\n");
    printf("  scan APIs                     显式传入 data、len 和 capacity\n");
    printf("  ms_logical_*                  使用 --api <接口> -h 查看详细格式\n");
    printf("  ms_set_*_allocator            <alloc> <calloc> <free>，各为 custom|default\n");
}

int demo_print_api_cli_extended_help(const char *prog, const char *api)
{
    if (cli_same(api, "ms_create_database") || cli_same(api, "ms_free_database") ||
        cli_same(api, "ms_handler_init") || cli_same(api, "ms_handler_destroy")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary_buckets>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_set_hash_func")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary_buckets> <custom|null>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_compile_database_multi_ext") ||
        cli_same(api, "ms_compile_multi_ext")) {
        printf("\nCLI 参数调用：\n  %s --api %s %s"
               "<expressions...> <flags...> <ids...> "
               "<ext_flags min_offset max_offset>... <elements>\n",
               prog, api,
               cli_same(api, "ms_compile_database_multi_ext") ?
               "<hash|ac> <buckets> <secondary> " : "");
        printf("ext_flags: 0=无约束, 1=MIN_OFFSET, 2=MAX_OFFSET, 3=MIN|MAX。\n");
        return 1;
    }
    if (cli_same(api, "ms_find_database_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s <alias> <key_len> <key> "
               "<hash|ac> <buckets> <secondary> <expressions...> "
               "<flags...> <ids...> <elements>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_scan_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <data> <len> <id_capacity> "
               "<hash|ac> <buckets> <secondary> <expressions...> "
               "<flags...> <ids...> <elements>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_flush_stream") || cli_same(api, "ms_close_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <id_capacity> <hash|ac> "
               "<buckets> <secondary> <expressions...> <flags...> "
               "<ids...> <elements>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_reset_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary> <expressions...> <flags...> <ids...> "
               "<elements>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_add_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary> <alias key_len key>... <rule_num>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_find_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s <alias> <key_len> <key> "
               "<hash|ac> <buckets> <secondary> "
               "<alias key_len key>...\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_remove_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s <alias> <key_len> <key|-> "
               "<rule_num> <hash|ac> <buckets> <secondary> "
               "<alias key_len key>...\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_compile_multi") || cli_same(api, "ms_add_exprs") ||
        cli_same(api, "ms_open_handler_stream") ||
        cli_same(api, "ms_close_handler_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s "
               "<expressions...> <%s...> <%s...> <elements>\n",
               prog, api,
               cli_same(api, "ms_add_exprs") ? "ids" : "flags",
               cli_same(api, "ms_add_exprs") ? "flags" : "ids");
        return 1;
    }
    if (cli_same(api, "ms_scan_expr") ||
        cli_same(api, "ms_compile_scan_ac_combinations") ||
        cli_same(api, "ms_scan_expr_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <data> <len> <id_capacity> "
               "<expressions...> <flags...> <ids...> <elements>\n",
               prog, api);
        return 1;
    }
    if (cli_same(api, "ms_compile_eval_combinations")) {
        printf("\nCLI 参数调用：\n  %s --api %s <alias_csv> <alias_count> "
               "<id_capacity> <expressions...> <flags...> <ids...> "
               "<elements>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_scan_rule_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <data> <len> "
               "<expressions...> <flags...> <ids...> <elements>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_scan_expr_stream_flush")) {
        printf("\nCLI 参数调用：\n  %s --api %s <id_capacity> "
               "<expressions...> <flags...> <ids...> <elements>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_logical_parse") ||
        cli_same(api, "ms_logical_tree_destroy")) {
        printf("\nCLI 参数调用：\n  %s --api %s <expression>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_logical_set") || cli_same(api, "ms_logical_get")) {
        printf("\nCLI 参数调用：\n  %s --api %s <assignments|-> "
               "<logical_count> <key>%s\n",
               prog, api, cli_same(api, "ms_logical_set") ? " <0|1>" : "");
        return 1;
    }
    if (cli_same(api, "ms_logical_clear")) {
        printf("\nCLI 参数调用：\n  %s --api %s <assignments|-> "
               "<logical_count>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_logical_eval")) {
        printf("\nCLI 参数调用：\n  %s --api %s <expression> "
               "<assignments|-> <logical_count>\n", prog, api);
        return 1;
    }
    if (cli_same(api, "ms_logical_eval_inplace")) {
        printf("\nCLI 参数调用：\n  %s --api %s <result_id> "
               "<not|and|or> <left> <right> <assignments> "
               "<logical_count>\n", prog, api);
        return 1;
    }
    if (strstr(api, "allocator") != NULL) {
        printf("\nCLI 参数调用：\n  %s --api %s "
               "<alloc:custom|default> <calloc:custom|default> "
               "<free:custom|default>\n", prog, api);
        return 1;
    }
    return 0;
}

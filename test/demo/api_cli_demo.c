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
    ms_rule_t *rules;
    uint32_t count;
} cli_rule_spec_t;

static size_t g_cli_allocs;
static size_t g_cli_callocs;
static size_t g_cli_frees;

static void cli_print_ret(const char *api, int ret)
{
    printf("%s ret=%d(%s)\n", api, ret, demo_ret_name(ret));
}

static int cli_parse_capacity(const char *text, uint32_t *capacity)
{
    if (demo_parse_u32(text, capacity) != 0 || *capacity > CLI_MAX_OUTPUT_CAPACITY) {
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

static int cli_parse_db_prefix(int argc, char **argv, ms_matcher_type *type,
                               uint32_t *buckets, uint32_t *secondary)
{
    return argc >= 3 && demo_parse_matcher(argv[0], type) == 0 &&
           demo_parse_u32(argv[1], buckets) == 0 &&
           demo_parse_u32(argv[2], secondary) == 0 ? 0 : -1;
}

static int cli_compile_db_call(int argc, char **argv, ms_database_t **db,
                               int *called)
{
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    demo_expr_spec_t spec = {0};
    int ret;

    if (called != NULL) *called = 0;
    if (cli_parse_db_prefix(argc, argv, &type, &buckets, &secondary) != 0 ||
        demo_parse_expr_spec(argc - 3, argv + 3, 0, 0, &spec) != 0) {
        return MS_INVALID_PARA;
    }
    if (called != NULL) *called = 1;
    ret = ms_compile_database_multi(db, type, buckets, secondary,
                                    spec.expressions, spec.flags, spec.ids,
                                    spec.count);
    demo_free_expr_spec(&spec);
    return ret;
}

static int cli_compile_db(int argc, char **argv, ms_database_t **db)
{
    return cli_compile_db_call(argc, argv, db, NULL);
}

static int cli_compile_handler(int argc, char **argv, ms_handler_t *handler,
                               int extended, int add_exprs)
{
    demo_expr_spec_t spec = {0};
    int ret;

    ret = ms_handler_init(handler, MS_MATCHER_TYPE_AC, 0, 0);
    if (ret != MS_SUCCESS) return ret;
    if (demo_parse_expr_spec(argc, argv, extended, add_exprs, &spec) != 0) {
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
    demo_free_expr_spec(&spec);
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
        if (demo_parse_u32(argv[i * 3u], &spec->rules[i].alias) != 0 ||
            demo_parse_u32(argv[i * 3u + 1u], &spec->rules[i].key_len) != 0 ||
            (!demo_same(key, "-") &&
             spec->rules[i].key_len > strlen(key))) {
            free(spec->rules);
            (void)memset(spec, 0, sizeof(*spec));
            return -1;
        }
        spec->rules[i].key = demo_same(key, "-") ? NULL : (uint8_t *)key;
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

    if (text == NULL || text[0] == '\0' || demo_same(text, "-")) return MS_SUCCESS;
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
    if (demo_same(api, "ms_create_database")) {
        cli_print_ret(api, ret);
        printf("output.db=%s\n", db == NULL ? "NULL" : "CREATED");
        if (db != NULL) cleanup_ret = ms_free_database(db);
        printf("cleanup.ms_free_database ret=%d(%s)\n",
               cleanup_ret, demo_ret_name(cleanup_ret));
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
    return demo_result_from_ret(ret);
}

static int cli_compile_database_ext(int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    demo_expr_spec_t spec = {0};
    int ret;
    int cleanup_ret = MS_SUCCESS;

    if (cli_parse_db_prefix(argc, argv, &type, &buckets, &secondary) != 0 ||
        demo_parse_expr_spec(argc - 3, argv + 3, 1, 0, &spec) != 0) {
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
           cleanup_ret, demo_ret_name(cleanup_ret));
    demo_free_expr_spec(&spec);
    return ret == MS_SUCCESS && db != NULL &&
           cleanup_ret == MS_SUCCESS ? 0 : 1;
}

static int cli_find_database_rule(int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_rule_t query;
    int ret;

    if (argc < 10 || demo_parse_u32(argv[0], &query.alias) != 0 ||
        demo_parse_u32(argv[1], &query.key_len) != 0 ||
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
               cleanup_ret, demo_ret_name(cleanup_ret));
    }
    return demo_result_from_ret(ret);
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

    if (demo_same(api, "ms_scan_stream")) {
        if (argc < 10 || demo_parse_u32(argv[1], &data_len) != 0 ||
            data_len > strlen(argv[0]) ||
            cli_parse_capacity(argv[2], &capacity) != 0) {
            printf("用法：--api ms_scan_stream <data> <len> <id_capacity> "
                   "<hash|ac> <buckets> <secondary> "
                   "<expressions...> <flags...> <ids...> <elements>\n");
            return 1;
        }
        data = argv[0];
        spec_offset = 3;
    } else if (demo_same(api, "ms_flush_stream") ||
               demo_same(api, "ms_close_stream")) {
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
    if (demo_same(api, "ms_scan_stream")) {
        ret = ms_scan_stream(stream, (const uint8_t *)data, data_len,
                             scratch, ids, &count, capacity);
    } else if (demo_same(api, "ms_flush_stream")) {
        count = 0;
        ret = ms_flush_stream(stream, scratch, ids, &count, capacity);
    } else if (demo_same(api, "ms_close_stream")) {
        count = 0;
        ret = ms_close_stream(stream, scratch, ids, &count, capacity);
        stream = NULL;
    } else if (demo_same(api, "ms_reset_stream")) {
        ret = ms_reset_stream(stream);
        cli_print_ret(api, ret);
    }
    if (!demo_same(api, "ms_reset_stream")) {
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
           cleanup_ret, demo_ret_name(cleanup_ret));
    return demo_result_from_ret(ret) == 0 && cleanup_ret == MS_SUCCESS ? 0 : 1;
}

static int cli_handler_lifecycle(const char *api, int argc, char **argv)
{
    ms_handler_t handler = {0};
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    int ret;

    if (argc != (demo_same(api, "ms_set_hash_func") ? 4 : 3) ||
        cli_parse_db_prefix(argc, argv, &type, &buckets, &secondary) != 0) {
        printf("用法：--api %s <hash|ac> <buckets> <secondary_buckets>%s\n",
               api, demo_same(api, "ms_set_hash_func") ?
               " <custom|null>" : "");
        return 1;
    }
    ret = ms_handler_init(&handler, type, buckets, secondary);
    if (demo_same(api, "ms_handler_init")) {
        cli_print_ret(api, ret);
    } else if (ret == MS_SUCCESS && demo_same(api, "ms_set_hash_func")) {
        if (!demo_same(argv[3], "custom") && !demo_same(argv[3], "null")) {
            ret = MS_INVALID_PARA;
        } else {
            ret = ms_set_hash_func(&handler,
                                   demo_same(argv[3], "custom") ?
                                   cli_hash : NULL);
        }
        cli_print_ret(api, ret);
    } else if (ret == MS_SUCCESS) {
        ret = ms_handler_destroy(&handler);
        (void)memset(&handler, 0, sizeof(handler));
        cli_print_ret(api, ret);
    }
    if (handler.ms_matcher != NULL) (void)ms_handler_destroy(&handler);
    return demo_result_from_ret(ret);
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
    int prefix = demo_same(api, "ms_add_rule") ? 0 :
                 demo_same(api, "ms_find_rule") ? 3 : 4;
    int ret;

    if (argc < prefix + 5 ||
        cli_parse_db_prefix(argc - prefix, argv + prefix,
                            &type, &buckets, &secondary) != 0 ||
        cli_parse_rules(argc - prefix - 3 -
                        (demo_same(api, "ms_add_rule") ? 1 : 0),
                        argv + prefix + 3, &spec) != 0 ||
        (demo_same(api, "ms_add_rule") &&
         (demo_parse_u32(argv[argc - 1], &rule_num) != 0 ||
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
                          demo_same(api, "ms_add_rule") ?
                          rule_num : spec.count);
    }
    if (ret != MS_SUCCESS || demo_same(api, "ms_add_rule")) {
        cli_print_ret(api, ret);
        goto cleanup;
    }
    if (!demo_same(api, "ms_add_rule") &&
        (demo_parse_u32(argv[0], &target.alias) != 0 ||
         demo_parse_u32(argv[1], &target.key_len) != 0 ||
         (!demo_same(argv[2], "-") && target.key_len > strlen(argv[2])))) {
        ret = MS_INVALID_PARA;
        goto cleanup;
    }
    if (demo_same(api, "ms_remove_rule") &&
        (demo_parse_u32(argv[3], &rule_num) != 0 || rule_num != 1)) {
        ret = MS_INVALID_PARA;
        goto cleanup;
    }
    target.key = demo_same(argv[2], "-") ? NULL : (uint8_t *)argv[2];
    if (demo_same(api, "ms_find_rule")) {
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
    return demo_same(api, "ms_remove_rule") ? (ret == MS_SUCCESS ? 0 : 1) :
           demo_result_from_ret(ret);
}

static int cli_compile_handler_target(const char *api, int argc, char **argv)
{
    ms_handler_t handler = {0};
    int extended = demo_same(api, "ms_compile_multi_ext");
    int add_exprs = demo_same(api, "ms_add_exprs");
    demo_expr_spec_t spec = {0};
    int ret;

    ret = ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0);
    if (ret != MS_SUCCESS) return 1;
    if (demo_parse_expr_spec(argc, argv, extended, add_exprs, &spec) != 0) {
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
    demo_free_expr_spec(&spec);
    (void)ms_handler_destroy(&handler);
    return demo_result_from_ret(ret);
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

    if (demo_same(api, "ms_open_handler_stream") ||
        demo_same(api, "ms_close_handler_stream")) {
        spec_offset = 0;
    } else if (demo_same(api, "ms_scan_rule_stream")) {
        if (argc < 6 || demo_parse_u32(argv[1], &data_len) != 0 ||
            data_len > strlen(argv[0])) {
            printf("用法：--api ms_scan_rule_stream <data> <len> "
                   "<expressions...> <flags...> <ids...> <elements>\n");
            return 1;
        }
        spec_offset = 2;
    } else if (demo_same(api, "ms_scan_expr_stream_flush")) {
        if (argc < 4 || cli_parse_capacity(argv[0], &capacity) != 0) {
            printf("用法：--api ms_scan_expr_stream_flush <id_capacity> "
                   "<expressions...> <flags...> <ids...> <elements>\n");
            return 1;
        }
        spec_offset = 1;
    } else {
        if (argc < 7 ||
            demo_parse_u32(argv[1],
                          demo_same(api, "ms_compile_eval_combinations") ?
                          &supplied_alias_count : &data_len) != 0 ||
            (!demo_same(api, "ms_compile_eval_combinations") &&
             data_len > strlen(argv[0])) ||
            cli_parse_capacity(argv[2], &capacity) != 0) {
            printf("用法：--api %s <%s> <%s> <id_capacity> "
                   "<expressions...> <flags...> <ids...> <elements>\n",
                   api,
                   demo_same(api, "ms_compile_eval_combinations") ?
                   "alias_csv" : "data",
                   demo_same(api, "ms_compile_eval_combinations") ?
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

    if (demo_same(api, "ms_scan_expr")) {
        ids = cli_alloc_output(capacity);
        ret = capacity != 0 && ids == NULL ? MS_MALLOC_FAIL :
              ms_scan_expr(&handler, (const uint8_t *)argv[0],
                           data_len, ids, &count, capacity);
    } else if (demo_same(api, "ms_compile_eval_combinations")) {
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
    } else if (demo_same(api, "ms_compile_scan_ac_combinations")) {
        ids = cli_alloc_output(capacity);
        ret = capacity != 0 && ids == NULL ? MS_MALLOC_FAIL :
              ms_compile_scan_ac_combinations(
                  &handler, (const uint8_t *)argv[0],
                  data_len, ids, &count, capacity);
    } else {
        ret = ms_open_handler_stream(&handler, &stream);
        if (ret != MS_SUCCESS) goto done;
        if (demo_same(api, "ms_open_handler_stream")) {
            /* The target call is the setup call above. */
        } else if (demo_same(api, "ms_scan_expr_stream")) {
            ids = cli_alloc_output(capacity);
            ret = capacity != 0 && ids == NULL ? MS_MALLOC_FAIL :
                  ms_scan_expr_stream(stream, (const uint8_t *)argv[0],
                                      data_len, ids, &count,
                                      capacity);
        } else if (demo_same(api, "ms_scan_rule_stream")) {
            ret = ms_scan_rule_stream(stream, (const uint8_t *)argv[0],
                                      data_len);
        } else if (demo_same(api, "ms_scan_expr_stream_flush")) {
            ids = cli_alloc_output(capacity);
            ret = capacity != 0 && ids == NULL ? MS_MALLOC_FAIL :
                  ms_scan_expr_stream_flush(stream, ids, &count, capacity);
        } else if (demo_same(api, "ms_reset_stream")) {
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
    return demo_result_from_ret(ret);
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

    if (demo_same(api, "ms_logical_parse") ||
        demo_same(api, "ms_logical_tree_destroy")) {
        if (argc != 1) {
            printf("用法：--api %s <expression>\n", api);
            return 1;
        }
        ret = ms_logical_parse(argv[0], &tree);
        if (ret == MS_SUCCESS && demo_same(api, "ms_logical_tree_destroy")) {
            ms_logical_tree_destroy(&tree);
            ret = MS_SUCCESS;
        }
        cli_print_ret(api, ret);
        if (tree.ops != NULL) ms_logical_tree_destroy(&tree);
        return demo_result_from_ret(ret);
    }

    if (demo_same(api, "ms_logical_set") || demo_same(api, "ms_logical_get")) {
        int is_set = demo_same(api, "ms_logical_set");

        if (argc != (is_set ? 4 : 3) ||
            demo_parse_u32(argv[1], &logical_count) != 0 ||
            demo_parse_u32(argv[2], &key) != 0 ||
            (is_set &&
             (demo_parse_u32(argv[3], &raw_value) != 0 || raw_value > 1u))) {
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
        return demo_result_from_ret(ret);
    }

    if (demo_same(api, "ms_logical_clear")) {
        if (argc != 2 || demo_parse_u32(argv[1], &logical_count) != 0) {
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
        return demo_result_from_ret(ret);
    }

    if (demo_same(api, "ms_logical_eval")) {
        if (argc != 3 || demo_parse_u32(argv[2], &logical_count) != 0) {
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
        return demo_result_from_ret(ret);
    }

    if (demo_same(api, "ms_logical_eval_inplace")) {
        ms_logical_op_t op;
        ms_logical_tree_t direct;
        uint32_t operation;

        if (argc != 6 || demo_parse_u32(argv[5], &logical_count) != 0 ||
            demo_parse_u32(argv[0], &op.id) != 0 ||
            demo_parse_u32(argv[2], &op.left) != 0 ||
            demo_parse_u32(argv[3], &op.right) != 0) {
            printf("用法：--api ms_logical_eval_inplace <result_id> "
                   "<not|and|or> <left> <right> <assignments> "
                   "<logical_count>\n");
            return 1;
        }
        operation = demo_same(argv[1], "not") ? MS_LOGICAL_OP_NOT :
                    demo_same(argv[1], "and") ? MS_LOGICAL_OP_AND :
                    demo_same(argv[1], "or") ? MS_LOGICAL_OP_OR : UINT32_MAX;
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
        return demo_result_from_ret(ret);
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
        (!demo_same(argv[0], "custom") && !demo_same(argv[0], "default")) ||
        (!demo_same(argv[1], "custom") && !demo_same(argv[1], "default")) ||
        (!demo_same(argv[2], "custom") && !demo_same(argv[2], "default"))) {
        printf("用法：--api %s <alloc:custom|default> "
               "<calloc:custom|default> <free:custom|default>\n", api);
        return 1;
    }
    custom_alloc = demo_same(argv[0], "custom");
    custom_calloc = demo_same(argv[1], "custom");
    custom_free = demo_same(argv[2], "custom");
    any_custom = custom_alloc || custom_calloc || custom_free;
    g_cli_allocs = g_cli_callocs = g_cli_frees = 0;
    if (demo_same(api, "ms_set_allocator")) {
        ret = ms_set_allocator(custom_alloc ? cli_alloc : NULL,
                               custom_calloc ? cli_calloc : NULL,
                               custom_free ? cli_free : NULL);
    } else if (demo_same(api, "ms_set_database_allocator")) {
        ret = ms_set_database_allocator(custom_alloc ? cli_alloc : NULL,
                                        custom_calloc ? cli_calloc : NULL,
                                        custom_free ? cli_free : NULL);
    } else if (demo_same(api, "ms_set_scratch_allocator")) {
        ret = ms_set_scratch_allocator(custom_alloc ? cli_alloc : NULL,
                                       custom_calloc ? cli_calloc : NULL,
                                       custom_free ? cli_free : NULL);
    } else if (demo_same(api, "ms_set_stream_allocator")) {
        ret = ms_set_stream_allocator(custom_alloc ? cli_alloc : NULL,
                                      custom_calloc ? cli_calloc : NULL,
                                      custom_free ? cli_free : NULL);
    } else if (demo_same(api, "ms_set_misc_allocator")) {
        ret = ms_set_misc_allocator(custom_alloc ? cli_alloc : NULL,
                                    custom_calloc ? cli_calloc : NULL,
                                    custom_free ? cli_free : NULL);
    } else {
        return -1;
    }
    cli_print_ret(api, ret);
    if (ret != MS_SUCCESS) return demo_result_from_ret(ret);

    /*
     * A setter-only call does not prove allocator routing. Exercise the object
     * category named by the API while the callback is installed.
     */
    if (any_custom && !demo_same(api, "ms_set_misc_allocator")) {
        exercise_ret = ms_compile_database_multi(
            &db, MS_MATCHER_TYPE_AC, 0, 0,
            (const char *const[]){"abc"}, NULL, (const uint32_t[]){101}, 1);
        if (exercise_ret == MS_SUCCESS &&
            (demo_same(api, "ms_set_allocator") ||
             demo_same(api, "ms_set_scratch_allocator") ||
             demo_same(api, "ms_set_stream_allocator"))) {
            exercise_ret = ms_alloc_scratch(db, &scratch);
        }
        if (exercise_ret == MS_SUCCESS &&
            (demo_same(api, "ms_set_allocator") ||
             demo_same(api, "ms_set_stream_allocator"))) {
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
               routed ? "PASS" : "FAIL", demo_ret_name(exercise_ret));
    } else if (any_custom) {
        printf("route=N/A(misc_has_no_public_consumer)\n");
    }
    printf("malloc=%zu calloc=%zu free=%zu\n",
           g_cli_allocs, g_cli_callocs, g_cli_frees);
    /* A CLI invocation must never leave process-global allocators changed. */
    if (any_custom) {
        if (demo_same(api, "ms_set_allocator")) {
            (void)ms_set_allocator(NULL, NULL, NULL);
        } else if (demo_same(api, "ms_set_database_allocator")) {
            (void)ms_set_database_allocator(NULL, NULL, NULL);
        } else if (demo_same(api, "ms_set_scratch_allocator")) {
            (void)ms_set_scratch_allocator(NULL, NULL, NULL);
        } else if (demo_same(api, "ms_set_stream_allocator")) {
            (void)ms_set_stream_allocator(NULL, NULL, NULL);
        } else {
            (void)ms_set_misc_allocator(NULL, NULL, NULL);
        }
    }
    return routed ? demo_result_from_ret(ret) : 1;
}

static int cli_database_object_api(const char *api, int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t ids[1] = {0};
    uint32_t count = 0;
    int called = 0;
    int ret = cli_compile_db_call(argc, argv, &db, &called);
    int cleanup_ret = MS_SUCCESS;

    if (demo_same(api, "ms_compile_database_multi")) {
        if (!called) {
            printf("ERROR stage=CLI api=%s called=NO\n", api);
            return 1;
        }
        cli_print_ret(api, ret);
        printf("output.db=%s\n", db == NULL ? "NULL" : "CREATED");
        if (db != NULL) cleanup_ret = ms_free_database(db);
        printf("cleanup.ms_free_database ret=%d(%s)\n",
               cleanup_ret, demo_ret_name(cleanup_ret));
        return ret == MS_SUCCESS && db != NULL &&
               cleanup_ret == MS_SUCCESS ? 0 : 1;
    }
    if (ret != MS_SUCCESS) {
        printf("PRECONDITION api=%s source=AUTO objects=database "
               "status=FAILED\n", api);
        cli_print_ret("setup.compile", ret);
        printf("%s called=NO\n", api);
        return 1;
    }
    if (demo_same(api, "ms_alloc_scratch")) {
        printf("PRECONDITION api=%s source=AUTO objects=database "
               "status=READY reason=STANDALONE_CALL\n", api);
        ret = ms_alloc_scratch(db, &scratch);
        cli_print_ret(api, ret);
        printf("output.scratch=%s\n", scratch == NULL ? "NULL" : "CREATED");
    } else if (demo_same(api, "ms_free_scratch")) {
        ret = ms_alloc_scratch(db, &scratch);
        printf("PRECONDITION api=%s source=AUTO objects=database,scratch "
               "status=%s reason=STANDALONE_CALL\n", api,
               ret == MS_SUCCESS ? "READY" : "FAILED");
        if (ret == MS_SUCCESS) {
            ret = ms_free_scratch(scratch);
            if (ret == MS_SUCCESS) scratch = NULL;
            cli_print_ret(api, ret);
            printf("output.scratch=%s\n",
                   ret == MS_SUCCESS ? "RELEASED" : "NOT_RELEASED");
        } else {
            cli_print_ret("setup.ms_alloc_scratch", ret);
            printf("%s called=NO\n", api);
        }
    } else {
        ret = ms_alloc_scratch(db, &scratch);
        printf("PRECONDITION api=%s source=AUTO objects=database "
               "status=%s cleanup_helper=scratch reason=STANDALONE_CALL\n",
               api, ret == MS_SUCCESS ? "READY" : "FAILED");
        if (ret == MS_SUCCESS) {
            ret = ms_open_stream(db, &stream);
            cli_print_ret(api, ret);
            printf("output.stream=%s\n",
                   stream == NULL ? "NULL" : "CREATED");
        } else {
            cli_print_ret("setup.ms_alloc_scratch", ret);
            printf("%s called=NO\n", api);
        }
    }
    if (stream != NULL) {
        cleanup_ret = ms_close_stream(stream, scratch, ids, &count, 1);
    }
    if (scratch != NULL) {
        int free_ret = ms_free_scratch(scratch);
        if (cleanup_ret == MS_SUCCESS) cleanup_ret = free_ret;
    }
    {
        int free_ret = ms_free_database(db);
        if (cleanup_ret == MS_SUCCESS) cleanup_ret = free_ret;
    }
    printf("CLEANUP source=AUTO ret=%d(%s)\n",
           cleanup_ret, demo_ret_name(cleanup_ret));
    return demo_result_from_ret(ret) == 0 &&
           cleanup_ret == MS_SUCCESS ? 0 : 1;
}

static int cli_scan_database(int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    uint32_t *ids = NULL;
    uint32_t len;
    uint32_t capacity;
    uint32_t count = 0;
    int ret;
    int cleanup_ret = MS_SUCCESS;

    if (argc < 9 || demo_parse_u32(argv[1], &len) != 0 ||
        cli_parse_capacity(argv[2], &capacity) != 0 ||
        len > strlen(argv[0])) {
        printf("ERROR stage=CLI api=ms_scan_database called=NO\n");
        return 1;
    }
    ret = cli_compile_db(argc - 3, argv + 3, &db);
    if (ret == MS_SUCCESS) ret = ms_alloc_scratch(db, &scratch);
    if (ret == MS_SUCCESS) {
        ids = cli_alloc_output(capacity);
        if (capacity != 0 && ids == NULL) ret = MS_MALLOC_FAIL;
    }
    if (ret != MS_SUCCESS) {
        printf("PRECONDITION api=ms_scan_database source=AUTO "
               "objects=database,scratch status=FAILED\n");
        cli_print_ret("setup", ret);
        printf("ms_scan_database called=NO\n");
    } else {
        printf("PRECONDITION api=ms_scan_database source=AUTO "
               "objects=database,scratch status=READY "
               "reason=STANDALONE_CALL\n");
        ret = ms_scan_database(db, (const uint8_t *)argv[0], len, scratch,
                               ids, &count, capacity);
        cli_print_ret("ms_scan_database", ret);
        cli_print_output(ids, count, capacity);
    }
    free(ids);
    if (scratch != NULL) cleanup_ret = ms_free_scratch(scratch);
    if (db != NULL) {
        int free_ret = ms_free_database(db);
        if (cleanup_ret == MS_SUCCESS) cleanup_ret = free_ret;
    }
    printf("CLEANUP source=AUTO ret=%d(%s)\n",
           cleanup_ret, demo_ret_name(cleanup_ret));
    return demo_result_from_ret(ret) == 0 &&
           cleanup_ret == MS_SUCCESS ? 0 : 1;
}

static int cli_remove_database_rule(int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    cli_rule_spec_t rules = {0};
    ms_matcher_type type;
    uint32_t rule_num;
    uint32_t capacity;
    uint32_t ids[4] = {0};
    uint32_t count = 0;
    const char *verify_data;
    int separator = -1;
    int db_offset;
    int ret;
    int called = 0;

    for (int i = 0; i < argc; i++) {
        if (demo_same(argv[i], "--")) {
            separator = i;
            break;
        }
    }
    if (separator <= 0 || separator % 3 != 0 ||
        separator + 4 >= argc ||
        demo_parse_u32(argv[separator + 1], &rule_num) != 0 ||
        rule_num > 1024u ||
        cli_parse_rules(separator, argv, &rules) != 0) {
        printf("ERROR stage=CLI api=ms_remove_database_rule called=NO\n");
        return 1;
    }
    if (rule_num != rules.count) {
        printf("ERROR stage=CLI api=ms_remove_database_rule "
               "reason=COUNT_MISMATCH serialized=%u rule_num=%u called=NO\n",
               rules.count, rule_num);
        free(rules.rules);
        return 1;
    }
    db_offset = separator + 4;
    if (argc - db_offset < 7 ||
        cli_parse_capacity(argv[separator + 3], &capacity) != 0 ||
        capacity > DEMO_ARRAY_SIZE(ids) ||
        demo_parse_matcher(argv[db_offset], &type) != 0) {
        printf("ERROR stage=CLI api=ms_remove_database_rule called=NO\n");
        free(rules.rules);
        return 1;
    }
    verify_data = argv[separator + 2];
    ret = cli_compile_db(argc - db_offset, argv + db_offset, &db);
    if (ret == MS_SUCCESS && type == MS_MATCHER_TYPE_AC) {
        ret = ms_alloc_scratch(db, &scratch);
        if (ret == MS_SUCCESS) ret = ms_open_stream(db, &stream);
    }
    printf("PRECONDITION api=ms_remove_database_rule source=AUTO "
           "objects=database%s status=%s reason=STANDALONE_CALL\n",
           type == MS_MATCHER_TYPE_AC ? ",scratch,stream" : "",
           ret == MS_SUCCESS ? "READY" : "FAILED");
    if (ret == MS_SUCCESS) {
        called = 1;
        ret = ms_remove_database_rule(db, rules.rules, rule_num);
    }
    if (called) {
        cli_print_ret("ms_remove_database_rule", ret);
    } else {
        cli_print_ret("setup", ret);
        printf("ms_remove_database_rule called=NO\n");
    }
    if (ret == MS_SUCCESS && scratch != NULL &&
        !demo_same(verify_data, "-")) {
        int verify_ret = ms_scan_database(
            db, (const uint8_t *)verify_data, (uint32_t)strlen(verify_data),
            scratch, ids, &count, capacity);
        printf("verify.block ret=%d(%s) ", verify_ret,
               demo_ret_name(verify_ret));
        cli_print_output(ids, count, capacity);
    }
    if (ret == MS_SUCCESS && stream != NULL) {
        int stale_ret;
        int reset_ret;
        count = 0;
        stale_ret = ms_scan_stream(stream, (const uint8_t *)"", 0, scratch,
                                   ids, &count, capacity);
        reset_ret = ms_reset_stream(stream);
        printf("verify.stream stale=%d(%s) reset=%d(%s)\n",
               stale_ret, demo_ret_name(stale_ret),
               reset_ret, demo_ret_name(reset_ret));
    }
    if (stream != NULL) {
        count = 0;
        (void)ms_close_stream(stream, scratch, ids, &count, capacity);
    }
    if (scratch != NULL) (void)ms_free_scratch(scratch);
    if (db != NULL) (void)ms_free_database(db);
    free(rules.rules);
    return demo_result_from_ret(ret);
}

int demo_run_api_command(const char *api, int argc, char **argv)
{
    if (argc == 0) {
        printf("missing parameters: %s\n", api);
        return 1;
    }
    if (demo_same(api, "ms_compile_database_multi") ||
        demo_same(api, "ms_alloc_scratch") ||
        demo_same(api, "ms_free_scratch") ||
        demo_same(api, "ms_open_stream")) {
        return cli_database_object_api(api, argc, argv);
    }
    if (demo_same(api, "ms_scan_database")) {
        return cli_scan_database(argc, argv);
    }
    if (demo_same(api, "ms_remove_database_rule")) {
        return cli_remove_database_rule(argc, argv);
    }
    if (demo_same(api, "ms_create_database") ||
        demo_same(api, "ms_free_database")) {
        return cli_database_lifecycle(api, argc, argv);
    }
    if (demo_same(api, "ms_compile_database_multi_ext")) {
        return cli_compile_database_ext(argc, argv);
    }
    if (demo_same(api, "ms_find_database_rule")) {
        return cli_find_database_rule(argc, argv);
    }
    if (demo_same(api, "ms_scan_stream") ||
        demo_same(api, "ms_flush_stream") ||
        demo_same(api, "ms_close_stream") ||
        demo_same(api, "ms_reset_stream")) {
        return cli_database_stream_api(api, argc, argv);
    }
    if (demo_same(api, "ms_handler_init") ||
        demo_same(api, "ms_handler_destroy") ||
        demo_same(api, "ms_set_hash_func")) {
        return cli_handler_lifecycle(api, argc, argv);
    }
    if (demo_same(api, "ms_add_rule") ||
        demo_same(api, "ms_find_rule") ||
        demo_same(api, "ms_remove_rule")) {
        return cli_handler_rule_api(api, argc, argv);
    }
    if (demo_same(api, "ms_compile_multi") ||
        demo_same(api, "ms_compile_multi_ext") ||
        demo_same(api, "ms_add_exprs")) {
        return cli_compile_handler_target(api, argc, argv);
    }
    if (demo_same(api, "ms_scan_expr") ||
        demo_same(api, "ms_compile_eval_combinations") ||
        demo_same(api, "ms_compile_scan_ac_combinations") ||
        demo_same(api, "ms_open_handler_stream") ||
        demo_same(api, "ms_scan_expr_stream") ||
        demo_same(api, "ms_scan_rule_stream") ||
        demo_same(api, "ms_scan_expr_stream_flush") ||
        demo_same(api, "ms_close_handler_stream")) {
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

int demo_print_api_command_help(const char *prog, const char *api)
{
    if (demo_same(api, "ms_compile_database_multi") ||
        demo_same(api, "ms_alloc_scratch") ||
        demo_same(api, "ms_free_scratch") ||
        demo_same(api, "ms_open_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary> <expressions...> <flags...> <ids...> "
               "<elements>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_scan_database")) {
        printf("\nCLI 参数调用：\n  %s --api %s <data> <len> "
               "<id_capacity> <hash|ac> <buckets> <secondary> "
               "<expressions...> <flags...> <ids...> <elements>\n",
               prog, api);
        return 1;
    }
    if (demo_same(api, "ms_remove_database_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s "
               "<alias key_len key|->... -- <rule_num> "
               "<verify_data|-> <capacity> <database fixture...>\n",
               prog, api);
        return 1;
    }
    if (demo_same(api, "ms_create_database") || demo_same(api, "ms_free_database") ||
        demo_same(api, "ms_handler_init") || demo_same(api, "ms_handler_destroy")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary_buckets>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_set_hash_func")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary_buckets> <custom|null>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_compile_database_multi_ext") ||
        demo_same(api, "ms_compile_multi_ext")) {
        printf("\nCLI 参数调用：\n  %s --api %s %s"
               "<expressions...> <flags...> <ids...> "
               "<ext_flags min_offset max_offset>... <elements>\n",
               prog, api,
               demo_same(api, "ms_compile_database_multi_ext") ?
               "<hash|ac> <buckets> <secondary> " : "");
        printf("ext_flags: 0=无约束, 1=MIN_OFFSET, 2=MAX_OFFSET, 3=MIN|MAX。\n");
        return 1;
    }
    if (demo_same(api, "ms_find_database_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s <alias> <key_len> <key> "
               "<hash|ac> <buckets> <secondary> <expressions...> "
               "<flags...> <ids...> <elements>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_scan_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <data> <len> <id_capacity> "
               "<hash|ac> <buckets> <secondary> <expressions...> "
               "<flags...> <ids...> <elements>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_flush_stream") || demo_same(api, "ms_close_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <id_capacity> <hash|ac> "
               "<buckets> <secondary> <expressions...> <flags...> "
               "<ids...> <elements>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_reset_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary> <expressions...> <flags...> <ids...> "
               "<elements>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_add_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s <hash|ac> <buckets> "
               "<secondary> <alias key_len key>... <rule_num>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_find_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s <alias> <key_len> <key> "
               "<hash|ac> <buckets> <secondary> "
               "<alias key_len key>...\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_remove_rule")) {
        printf("\nCLI 参数调用：\n  %s --api %s <alias> <key_len> <key|-> "
               "<rule_num> <hash|ac> <buckets> <secondary> "
               "<alias key_len key>...\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_compile_multi") || demo_same(api, "ms_add_exprs") ||
        demo_same(api, "ms_open_handler_stream") ||
        demo_same(api, "ms_close_handler_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s "
               "<expressions...> <%s...> <%s...> <elements>\n",
               prog, api,
               demo_same(api, "ms_add_exprs") ? "ids" : "flags",
               demo_same(api, "ms_add_exprs") ? "flags" : "ids");
        return 1;
    }
    if (demo_same(api, "ms_scan_expr") ||
        demo_same(api, "ms_compile_scan_ac_combinations") ||
        demo_same(api, "ms_scan_expr_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <data> <len> <id_capacity> "
               "<expressions...> <flags...> <ids...> <elements>\n",
               prog, api);
        return 1;
    }
    if (demo_same(api, "ms_compile_eval_combinations")) {
        printf("\nCLI 参数调用：\n  %s --api %s <alias_csv> <alias_count> "
               "<id_capacity> <expressions...> <flags...> <ids...> "
               "<elements>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_scan_rule_stream")) {
        printf("\nCLI 参数调用：\n  %s --api %s <data> <len> "
               "<expressions...> <flags...> <ids...> <elements>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_scan_expr_stream_flush")) {
        printf("\nCLI 参数调用：\n  %s --api %s <id_capacity> "
               "<expressions...> <flags...> <ids...> <elements>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_logical_parse") ||
        demo_same(api, "ms_logical_tree_destroy")) {
        printf("\nCLI 参数调用：\n  %s --api %s <expression>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_logical_set") || demo_same(api, "ms_logical_get")) {
        printf("\nCLI 参数调用：\n  %s --api %s <assignments|-> "
               "<logical_count> <key>%s\n",
               prog, api, demo_same(api, "ms_logical_set") ? " <0|1>" : "");
        return 1;
    }
    if (demo_same(api, "ms_logical_clear")) {
        printf("\nCLI 参数调用：\n  %s --api %s <assignments|-> "
               "<logical_count>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_logical_eval")) {
        printf("\nCLI 参数调用：\n  %s --api %s <expression> "
               "<assignments|-> <logical_count>\n", prog, api);
        return 1;
    }
    if (demo_same(api, "ms_logical_eval_inplace")) {
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

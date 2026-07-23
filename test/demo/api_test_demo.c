/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Public API metadata and parameterized CLI entry.
 */
#include "demo_common.h"
#include "api_cli_demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ms.h"
#include "ms_runtime.h"

typedef struct {
    const char *name;
    const char *module;
    const char *summary;
} demo_api_t;

typedef struct {
    const char *key;
    const char *title;
    const char *flow;
} demo_module_t;

static const demo_module_t g_modules[] = {
    {"database", "功能 3-1：Database 编译与生命周期",
     "compile -> database -> free"},
    {"block", "功能 3-1/3-2：AC Block 扫描与组合",
     "database -> scratch -> scan_database -> ids"},
    {"stream", "功能 3-3：AC Stream 扫描",
     "database + scratch -> open -> scan/flush/reset/close"},
    {"delete", "功能 3-5：动态删除",
     "remove_database_rule -> scratch refresh / stream stale"},
    {"handler", "功能 3-1/3-5：Handler 兼容接口",
     "handler_init -> add/find/remove_rule -> handler_destroy"},
    {"expression", "功能 3-2：组合表达式兼容接口",
     "compile_multi -> block/stream combination evaluation"},
    {"logical", "功能 3-2：逻辑表达式",
     "logical_parse -> set/get/eval -> tree_destroy"},
    {"allocator", "功能 3-4：分类内存分配",
     "set allocator -> create/free corresponding object"}
};

static const demo_api_t g_apis[] = {
    {"ms_create_database", "database", "创建 database"},
    {"ms_compile_database_multi", "database", "编译普通或组合规则 database"},
    {"ms_compile_database_multi_ext", "database", "编译带 offset 的 database"},
    {"ms_free_database", "database", "释放 database"},
    {"ms_alloc_scratch", "block", "分配 scratch"},
    {"ms_free_scratch", "block", "释放 scratch"},
    {"ms_scan_database", "block", "执行 Block 扫描"},
    {"ms_find_database_rule", "database", "查询 database 规则"},
    {"ms_remove_database_rule", "delete", "批量删除 database 规则"},
    {"ms_open_stream", "stream", "打开 database stream"},
    {"ms_scan_stream", "stream", "扫描一个 chunk"},
    {"ms_flush_stream", "stream", "执行 stream 结束求值"},
    {"ms_close_stream", "stream", "结束并关闭 stream"},
    {"ms_reset_stream", "stream", "复位 stream 状态"},
    {"ms_handler_init", "handler", "初始化 handler"},
    {"ms_handler_destroy", "handler", "销毁 handler"},
    {"ms_set_hash_func", "handler", "设置 HASH 回调"},
    {"ms_add_rule", "handler", "添加固定规则"},
    {"ms_find_rule", "handler", "查询固定规则"},
    {"ms_remove_rule", "handler", "删除固定规则"},
    {"ms_compile_multi", "expression", "编译表达式"},
    {"ms_compile_multi_ext", "expression", "编译带 offset 的表达式"},
    {"ms_add_exprs", "expression", "添加表达式"},
    {"ms_scan_expr", "expression", "执行 handler Block 扫描"},
    {"ms_compile_eval_combinations", "expression", "使用 alias 求值组合"},
    {"ms_compile_scan_ac_combinations", "expression", "扫描并返回组合 ID"},
    {"ms_open_handler_stream", "expression", "打开 handler stream"},
    {"ms_scan_expr_stream", "expression", "执行 handler Stream 扫描"},
    {"ms_scan_rule_stream", "expression", "执行兼容 Stream 扫描"},
    {"ms_scan_expr_stream_flush", "expression", "刷新 handler stream"},
    {"ms_close_handler_stream", "expression", "关闭 handler stream"},
    {"ms_logical_parse", "logical", "解析逻辑表达式"},
    {"ms_logical_tree_destroy", "logical", "释放逻辑树"},
    {"ms_logical_set", "logical", "设置 logical bit"},
    {"ms_logical_get", "logical", "读取 logical bit"},
    {"ms_logical_clear", "logical", "清空 logical vector"},
    {"ms_logical_eval", "logical", "计算逻辑树"},
    {"ms_logical_eval_inplace", "logical", "原地计算逻辑树"},
    {"ms_set_allocator", "allocator", "设置全部 allocator"},
    {"ms_set_database_allocator", "allocator", "设置 database allocator"},
    {"ms_set_scratch_allocator", "allocator", "设置 scratch allocator"},
    {"ms_set_stream_allocator", "allocator", "设置 stream allocator"},
    {"ms_set_misc_allocator", "allocator", "设置 misc allocator"}
};

static int same(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

static const demo_api_t *find_api(const char *name)
{
    uint32_t i;

    for (i = 0; i < DEMO_ARRAY_SIZE(g_apis); i++) {
        if (same(g_apis[i].name, name)) return &g_apis[i];
    }
    return NULL;
}

static const char *ret_name(int ret)
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
        default: return "UNKNOWN";
    }
}

static void print_ret(const char *name, int ret)
{
    printf("%s ret=%d(%s)\n", name, ret, ret_name(ret));
}

static void print_ids(const uint32_t *ids, uint32_t count)
{
    uint32_t i;

    printf("ids=");
    for (i = 0; i < count; i++) {
        printf("%s%u", i == 0 ? "" : ",", ids[i]);
    }
    printf("\n");
}

static int result_from_ret(int ret)
{
    return ret == MS_SUCCESS || ret == MS_FIND_FAIL ? 0 : 1;
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);

    if (end == text || *end != '\0' || parsed > UINT32_MAX) return -1;
    *value = (uint32_t)parsed;
    return 0;
}

static int parse_matcher(const char *text, ms_matcher_type *type)
{
    if (same(text, "hash") || same(text, "1")) {
        *type = MS_MATCHER_TYPE_HASH;
        return 0;
    }
    if (same(text, "ac") || same(text, "2")) {
        *type = MS_MATCHER_TYPE_AC;
        return 0;
    }
    return -1;
}

static int compile_db(int argc, char **argv, ms_database_t **db,
                      int *api_called)
{
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    uint32_t elements;
    uint32_t array_count;
    const char **expressions;
    uint32_t *ids;
    uint32_t *flags;
    uint32_t i;
    int ret;

    if (api_called != NULL) *api_called = 0;
    if (argc < 7 || (argc - 4) % 3 != 0 ||
        parse_matcher(argv[0], &type) != 0 ||
        parse_u32(argv[1], &buckets) != 0 ||
        parse_u32(argv[2], &secondary) != 0) {
        return MS_INVALID_PARA;
    }
    array_count = (uint32_t)(argc - 4) / 3u;
    if (parse_u32(argv[argc - 1], &elements) != 0) {
        printf("参数错误：elements 必须是 uint32_t。\n");
        return MS_INVALID_PARA;
    }
    if (elements != array_count) {
        printf("参数错误：序列化了 %u 条 expression/flag/id，但 "
               "elements=%u；两者必须完全相等。\n",
               array_count, elements);
        return MS_INVALID_PARA;
    }
    expressions = (const char **)calloc(array_count + 1u, sizeof(*expressions));
    ids = (uint32_t *)calloc(array_count, sizeof(*ids));
    flags = (uint32_t *)calloc(array_count, sizeof(*flags));
    if (expressions == NULL || ids == NULL || flags == NULL) {
        free(expressions);
        free(ids);
        free(flags);
        return MS_MALLOC_FAIL;
    }
    for (i = 0; i < array_count; i++) {
        expressions[i] = argv[3 + i];
        if (parse_u32(argv[3 + array_count + i], &flags[i]) != 0 ||
            parse_u32(argv[3 + array_count * 2u + i], &ids[i]) != 0) {
            free(expressions);
            free(ids);
            free(flags);
            return MS_INVALID_PARA;
        }
    }
    if (api_called != NULL) *api_called = 1;
    ret = ms_compile_database_multi(db, type, buckets, secondary,
                                    expressions, flags, ids, elements);
    free(expressions);
    free(ids);
    free(flags);
    return ret;
}

static void print_db_usage(const char *prog, const char *api)
{
    printf("%s --api %s <hash|ac> <buckets> <secondary> "
           "<expressions...> <flags...> <ids...> <elements>\n", prog, api);
}

void demo_list_api_tests(void)
{
    uint32_t i;

    printf("API\tMODULE\tDESCRIPTION\n");
    for (i = 0; i < DEMO_ARRAY_SIZE(g_apis); i++) {
        printf("%s\t%s\t%s\n", g_apis[i].name, g_apis[i].module,
               g_apis[i].summary);
    }
}

void demo_print_api_cli_help(const char *prog)
{
    printf("用法：%s --api <接口名> <参数...>\n", prog);
    printf("      %s --api <接口名> -h\n", prog);
    printf("必须显式提供参数；不再提供无参数自动构造测试。\n\n");
    demo_list_api_tests();
}

void demo_print_api_test_help(const char *prog, const char *api)
{
    const demo_api_t *entry = find_api(api);

    if (entry == NULL) {
        printf("unknown api: %s\n", api);
        return;
    }
    printf("%s [%s] %s\n", entry->name, entry->module, entry->summary);
    if (same(api, "ms_compile_database_multi") ||
        same(api, "ms_alloc_scratch") ||
        same(api, "ms_free_scratch") ||
        same(api, "ms_open_stream")) {
        print_db_usage(prog, api);
        return;
    }
    if (same(api, "ms_scan_database")) {
        printf("%s --api ms_scan_database <data> <len> <capacity> "
               "<hash|ac> <buckets> <secondary> "
               "<expressions...> <flags...> <ids...> <elements>\n", prog);
        return;
    }
    if (same(api, "ms_remove_database_rule")) {
        printf("%s --api ms_remove_database_rule "
               "<alias key_len key|->... -- <rule_num> "
               "<verify_data|-> <capacity> "
               "<hash|ac> <buckets> <secondary> "
               "<expressions...> <flags...> <ids...> <elements>\n", prog);
        return;
    }
    (void)demo_print_api_cli_extended_help(prog, api);
}

static int run_database_object_api(const char *api, int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t ids[1] = {0};
    uint32_t count = 0;
    int api_called = 0;
    int ret = compile_db(argc, argv, &db, &api_called);
    int cleanup_ret = MS_SUCCESS;

    if (same(api, "ms_compile_database_multi")) {
        if (!api_called) {
            printf("ms_compile_database_multi called=NO\n");
            return 1;
        }
        print_ret(api, ret);
        printf("output.db=%s\n", db == NULL ? "NULL" : "CREATED");
        if (db != NULL) cleanup_ret = ms_free_database(db);
        printf("cleanup.ms_free_database ret=%d(%s)\n",
               cleanup_ret, ret_name(cleanup_ret));
        return ret == MS_SUCCESS && db != NULL &&
               cleanup_ret == MS_SUCCESS ? 0 : 1;
    }
    if (ret != MS_SUCCESS) {
        printf("PRECONDITION api=%s source=AUTO objects=database "
               "status=FAILED\n", api);
        print_ret("setup.compile", ret);
        printf("%s called=NO\n", api);
        return 1;
    }
    if (same(api, "ms_alloc_scratch")) {
        printf("PRECONDITION api=ms_alloc_scratch source=AUTO "
               "objects=database status=READY reason=STANDALONE_CALL\n");
        ret = ms_alloc_scratch(db, &scratch);
        print_ret(api, ret);
        printf("output.scratch=%s\n", scratch == NULL ? "NULL" : "CREATED");
    } else if (same(api, "ms_free_scratch")) {
        ret = ms_alloc_scratch(db, &scratch);
        printf("PRECONDITION api=ms_free_scratch source=AUTO "
               "objects=database,scratch status=%s "
               "reason=STANDALONE_CALL\n",
               ret == MS_SUCCESS ? "READY" : "FAILED");
        if (ret == MS_SUCCESS) {
            ret = ms_free_scratch(scratch);
            if (ret == MS_SUCCESS) scratch = NULL;
            print_ret(api, ret);
            printf("output.scratch=%s\n",
                   ret == MS_SUCCESS ? "RELEASED" : "NOT_RELEASED");
        } else {
            print_ret("setup.ms_alloc_scratch", ret);
            printf("ms_free_scratch called=NO\n");
        }
    } else {
        ret = ms_alloc_scratch(db, &scratch);
        printf("PRECONDITION api=ms_open_stream source=AUTO "
               "objects=database status=%s cleanup_helper=scratch "
               "reason=STANDALONE_CALL\n",
               ret == MS_SUCCESS ? "READY" : "FAILED");
        if (ret == MS_SUCCESS) {
            ret = ms_open_stream(db, &stream);
            print_ret(api, ret);
            printf("output.stream=%s\n",
                   stream == NULL ? "NULL" : "CREATED");
        } else {
            print_ret("setup.ms_alloc_scratch", ret);
            printf("ms_open_stream called=NO\n");
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
           cleanup_ret, ret_name(cleanup_ret));
    return result_from_ret(ret) == 0 && cleanup_ret == MS_SUCCESS ? 0 : 1;
}

static int run_scan_database(int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    uint32_t *ids = NULL;
    uint32_t len;
    uint32_t capacity;
    uint32_t count = 0;
    int ret;
    int cleanup_ret = MS_SUCCESS;

    if (argc < 9 || parse_u32(argv[1], &len) != 0 ||
        parse_u32(argv[2], &capacity) != 0 ||
        len > strlen(argv[0]) || capacity > 65536u) {
        return 1;
    }
    ret = compile_db(argc - 3, argv + 3, &db, NULL);
    if (ret == MS_SUCCESS) ret = ms_alloc_scratch(db, &scratch);
    if (ret == MS_SUCCESS && capacity != 0) {
        ids = (uint32_t *)calloc(capacity, sizeof(*ids));
        if (ids == NULL) ret = MS_MALLOC_FAIL;
    }
    if (ret != MS_SUCCESS) {
        printf("PRECONDITION api=ms_scan_database source=AUTO "
               "objects=database,scratch status=FAILED "
               "reason=STANDALONE_CALL\n");
        print_ret("setup", ret);
        printf("ms_scan_database called=NO\n");
        free(ids);
        if (scratch != NULL) cleanup_ret = ms_free_scratch(scratch);
        if (db != NULL) {
            int free_ret = ms_free_database(db);
            if (cleanup_ret == MS_SUCCESS) cleanup_ret = free_ret;
        }
        printf("CLEANUP source=AUTO ret=%d(%s)\n",
               cleanup_ret, ret_name(cleanup_ret));
        return 1;
    }
    printf("PRECONDITION api=ms_scan_database source=AUTO "
           "objects=database,scratch status=READY "
           "reason=STANDALONE_CALL\n");
    ret = ms_scan_database(db, (const uint8_t *)argv[0], len, scratch,
                           ids, &count, capacity);
    print_ret("ms_scan_database", ret);
    printf("count=%u ", count);
    print_ids(ids, count < capacity ? count : capacity);
    free(ids);
    if (scratch != NULL) cleanup_ret = ms_free_scratch(scratch);
    if (db != NULL) {
        int free_ret = ms_free_database(db);
        if (cleanup_ret == MS_SUCCESS) cleanup_ret = free_ret;
    }
    printf("CLEANUP source=AUTO ret=%d(%s)\n",
           cleanup_ret, ret_name(cleanup_ret));
    return result_from_ret(ret) == 0 && cleanup_ret == MS_SUCCESS ? 0 : 1;
}

static int run_remove_database_rule(int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    ms_rule_t *rules = NULL;
    ms_matcher_type type;
    uint32_t rule_num;
    uint32_t rule_array_count;
    uint32_t capacity;
    uint32_t ids[4] = {0};
    uint32_t count = 0;
    const char *verify_data;
    int separator = -1;
    int db_offset;
    uint32_t i;
    int ret;
    int target_called = 0;

    for (i = 0; i < (uint32_t)argc; i++) {
        if (same(argv[i], "--")) {
            separator = (int)i;
            break;
        }
    }
    if (separator <= 0 || separator % 3 != 0 ||
        separator + 4 >= argc ||
        parse_u32(argv[separator + 1], &rule_num) != 0 ||
        rule_num > 1024u) {
        return 1;
    }
    rule_array_count = (uint32_t)separator / 3u;
    if (rule_num != rule_array_count) {
        printf("参数错误：序列化了 %u 条 rule，但 rule_num=%u；"
               "两者必须完全相等。\n", rule_array_count, rule_num);
        return 1;
    }
    db_offset = separator + 4;
    if (db_offset >= argc || argc - db_offset < 6 ||
        parse_u32(argv[separator + 3], &capacity) != 0 || capacity > 4 ||
        parse_matcher(argv[db_offset], &type) != 0) {
        return 1;
    }
    verify_data = argv[separator + 2];
    rules = (ms_rule_t *)calloc(rule_array_count, sizeof(*rules));
    if (rules == NULL) return 1;
    for (i = 0; i < rule_array_count; i++) {
        int offset = (int)i * 3;
        if (parse_u32(argv[offset], &rules[i].alias) != 0 ||
            parse_u32(argv[offset + 1], &rules[i].key_len) != 0 ||
            (!same(argv[offset + 2], "-") &&
             rules[i].key_len > strlen(argv[offset + 2]))) {
            free(rules);
            return 1;
        }
        rules[i].key = same(argv[offset + 2], "-") ? NULL :
                       (uint8_t *)argv[offset + 2];
    }
    ret = compile_db(argc - db_offset, argv + db_offset, &db, NULL);
    if (ret == MS_SUCCESS && type == MS_MATCHER_TYPE_AC) {
        ret = ms_alloc_scratch(db, &scratch);
        if (ret == MS_SUCCESS) ret = ms_open_stream(db, &stream);
    }
    printf("PRECONDITION api=ms_remove_database_rule source=AUTO "
           "objects=database%s status=%s reason=STANDALONE_CALL\n",
           type == MS_MATCHER_TYPE_AC ? ",scratch,stream" : "",
           ret == MS_SUCCESS ? "READY" : "FAILED");
    if (ret == MS_SUCCESS) {
        target_called = 1;
        ret = ms_remove_database_rule(db, rules, rule_num);
    }
    if (!target_called) {
        print_ret("setup", ret);
        printf("ms_remove_database_rule called=NO\n");
    } else {
        print_ret("ms_remove_database_rule", ret);
    }
    if (ret == MS_SUCCESS && scratch != NULL && !same(verify_data, "-")) {
        int verify_ret = ms_scan_database(
            db, (const uint8_t *)verify_data, (uint32_t)strlen(verify_data),
            scratch, ids, &count, capacity);
        printf("verify.block ret=%d count=%u ", verify_ret, count);
        print_ids(ids, count < capacity ? count : capacity);
    }
    if (ret == MS_SUCCESS && stream != NULL) {
        int stale_ret;
        int reset_ret;
        count = 0;
        stale_ret = ms_scan_stream(stream, (const uint8_t *)"", 0, scratch,
                                   ids, &count, capacity);
        reset_ret = ms_reset_stream(stream);
        printf("verify.stream stale=%d reset=%d\n", stale_ret, reset_ret);
    }
    if (stream != NULL) {
        count = 0;
        (void)ms_close_stream(stream, scratch, ids, &count, capacity);
    }
    if (scratch != NULL) (void)ms_free_scratch(scratch);
    if (db != NULL) (void)ms_free_database(db);
    free(rules);
    return result_from_ret(ret);
}

int demo_run_api_cli(const char *api, int argc, char **argv)
{
    int ret;

    if (find_api(api) == NULL) {
        printf("unknown api: %s\n", api);
        return 1;
    }
    if (argc == 0) {
        printf("missing parameters: %s\n", api);
        return 1;
    }
    if (same(api, "ms_compile_database_multi") ||
        same(api, "ms_alloc_scratch") ||
        same(api, "ms_free_scratch") ||
        same(api, "ms_open_stream")) {
        return run_database_object_api(api, argc, argv);
    }
    if (same(api, "ms_scan_database")) return run_scan_database(argc, argv);
    if (same(api, "ms_remove_database_rule")) {
        return run_remove_database_rule(argc, argv);
    }
    ret = demo_run_api_cli_extended(api, argc, argv);
    return ret < 0 ? 1 : ret;
}

void demo_list_modules(void)
{
    uint32_t i;

    printf("MODULE\tDESCRIPTION\n");
    for (i = 0; i < DEMO_ARRAY_SIZE(g_modules); i++) {
        printf("%s\t%s\n", g_modules[i].key, g_modules[i].title);
    }
}

int demo_show_module(const char *module)
{
    uint32_t i;
    uint32_t j;

    for (i = 0; i < DEMO_ARRAY_SIZE(g_modules); i++) {
        if (!same(g_modules[i].key, module)) continue;
        printf("%s\nflow: %s\napis:", g_modules[i].title, g_modules[i].flow);
        for (j = 0; j < DEMO_ARRAY_SIZE(g_apis); j++) {
            if (same(g_apis[j].module, module)) printf(" %s", g_apis[j].name);
        }
        printf("\n");
        return 0;
    }
    printf("unknown module: %s\n", module);
    return 1;
}

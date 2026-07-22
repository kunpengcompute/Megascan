/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Focused public API tests and module flow descriptions.
 */
#include "demo_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ms.h"
#include "ms_allocator.h"
#include "ms_compile.h"
#include "ms_expr.h"
#include "ms_handler.h"
#include "ms_logical.h"
#include "ms_runtime.h"

typedef int (*demo_api_test_fn)(const char *api);

typedef struct {
    const char *name;
    const char *module;
    const char *summary;
    demo_api_test_fn fn;
} demo_api_test_t;

typedef struct {
    const char *key;
    const char *title;
    const char *flow;
} demo_module_t;

static int demo_test_database_api(const char *api);
static int demo_test_handler_api(const char *api);
static int demo_test_expression_api(const char *api);
static int demo_test_logical_api(const char *api);
static int demo_test_allocator_api(const char *api);
static int demo_test_database_remove(const char *api);

static const demo_module_t g_modules[] = {
    {
        "database", "Database 编译与生命周期",
        "expressions/flags/ids\n"
        "  -> ms_compile_database_multi[_ext]\n"
        "  -> 参数校验 + compile context\n"
        "  -> HASH table 或 compact ACDB\n"
        "  -> ms_database_t\n"
        "  -> ms_free_database"
    },
    {
        "block", "AC Block 扫描",
        "ms_database_t\n"
        "  -> ms_alloc_scratch\n"
        "  -> ms_scan_database\n"
        "     -> AC 命中(alias, offset)\n"
        "     -> 普通/QUIET/组合求值\n"
        "     -> ids + id_count\n"
        "  -> ms_free_scratch"
    },
    {
        "stream", "AC Stream 扫描",
        "ms_database_t + ms_scratch_t\n"
        "  -> ms_open_stream\n"
        "  -> ms_scan_stream(chunk 1..N)\n"
        "     -> node/offset/组合状态提交或回滚\n"
        "  -> ms_flush_stream (结束求值)\n"
        "  -> ms_reset_stream 或 ms_close_stream"
    },
    {
        "delete", "AC Database 动态删除",
        "ms_remove_database_rule\n"
        "  -> clone compile context\n"
        "  -> 从 compact ACDB 临时恢复规则\n"
        "  -> 删除普通/组合规则并构建候选 ACDB\n"
        "  -> 全部成功后原子提交 + generation++\n"
        "  -> scratch 自动刷新 / stream stale 后 reset"
    },
    {
        "handler", "Handler 兼容接口",
        "ms_handler_init\n"
        "  -> ms_add_rule / ms_compile_multi[_ext]\n"
        "  -> ms_find_rule / ms_scan_expr\n"
        "  -> ms_remove_rule\n"
        "  -> ms_handler_destroy"
    },
    {
        "expression", "组合表达式运行",
        "普通规则 ID + ! & | 表达式\n"
        "  -> ms_logical_parse\n"
        "  -> 外部 ID 重映射为 lkey\n"
        "  -> lkey_to_ckeys 选择受影响组合\n"
        "  -> offset/SINGLEMATCH/QUIET\n"
        "  -> 普通 ID + 组合 ID"
    },
    {
        "logical", "独立逻辑模块",
        "ms_logical_parse\n"
        "  -> ms_logical_set/get\n"
        "  -> ms_logical_eval 或 ms_logical_eval_inplace\n"
        "  -> matched\n"
        "  -> ms_logical_clear + ms_logical_tree_destroy"
    },
    {
        "allocator", "分类内存分配",
        "ms_set_allocator 或分类 setter\n"
        "  -> database: 编译产物/matcher\n"
        "  -> scratch: 单次扫描工作区\n"
        "  -> stream: 跨 chunk 持续状态\n"
        "  -> misc: 其他输出\n"
        "对象释放完成后恢复默认 allocator"
    }
};

static const demo_api_test_t g_api_tests[] = {
    {"ms_create_database", "database", "创建并释放一个空 database。", demo_test_database_api},
    {"ms_compile_database_multi", "database", "编译普通 AC database。", demo_test_database_api},
    {"ms_compile_database_multi_ext", "database", "编译带 offset 的 ACCombo database。", demo_test_database_api},
    {"ms_free_database", "database", "释放完整 database。", demo_test_database_api},
    {"ms_alloc_scratch", "block", "为指定 database 分配 scratch。", demo_test_database_api},
    {"ms_free_scratch", "block", "释放 scratch。", demo_test_database_api},
    {"ms_scan_database", "block", "执行一次 ACCombo Block 扫描并校验 ID。", demo_test_database_api},
    {"ms_find_database_rule", "database", "执行 HASH 精确查询并校验 alias。", demo_test_database_api},
    {"ms_remove_database_rule", "delete", "执行普通/组合删除、scratch 刷新和 stream stale/reset 端到端测试。", demo_test_database_remove},
    {"ms_open_stream", "stream", "打开 database stream。", demo_test_database_api},
    {"ms_scan_stream", "stream", "执行跨 chunk 组合扫描。", demo_test_database_api},
    {"ms_flush_stream", "stream", "执行纯负组合结束求值。", demo_test_database_api},
    {"ms_close_stream", "stream", "Flush 并关闭 database stream。", demo_test_database_api},
    {"ms_reset_stream", "stream", "清除半匹配状态并复位 stream。", demo_test_database_api},

    {"ms_handler_init", "handler", "初始化 legacy handler。", demo_test_handler_api},
    {"ms_handler_destroy", "handler", "销毁 legacy handler。", demo_test_handler_api},
    {"ms_set_hash_func", "handler", "为 HASH handler 设置 hash 回调。", demo_test_handler_api},
    {"ms_add_rule", "handler", "向 HASH handler 添加固定 key。", demo_test_handler_api},
    {"ms_find_rule", "handler", "精确查找 HASH key 并校验 alias。", demo_test_handler_api},
    {"ms_remove_rule", "handler", "删除 HASH key 并验证不可再查找。", demo_test_handler_api},

    {"ms_compile_multi", "expression", "通过 handler 编译表达式。", demo_test_expression_api},
    {"ms_compile_multi_ext", "expression", "通过 handler 编译带扩展参数的组合。", demo_test_expression_api},
    {"ms_add_exprs", "expression", "通过兼容入口添加表达式。", demo_test_expression_api},
    {"ms_scan_expr", "expression", "执行 handler Block 表达式扫描。", demo_test_expression_api},
    {"ms_compile_eval_combinations", "expression", "根据 alias 集合计算组合。", demo_test_expression_api},
    {"ms_compile_scan_ac_combinations", "expression", "扫描并只返回组合 ID。", demo_test_expression_api},
    {"ms_open_handler_stream", "expression", "打开 handler stream。", demo_test_expression_api},
    {"ms_scan_expr_stream", "expression", "执行 handler Stream 表达式扫描。", demo_test_expression_api},
    {"ms_scan_rule_stream", "expression", "执行不返回 ID 的兼容 Stream 扫描。", demo_test_expression_api},
    {"ms_scan_expr_stream_flush", "expression", "刷新 handler stream 的纯负组合。", demo_test_expression_api},
    {"ms_close_handler_stream", "expression", "关闭 handler stream。", demo_test_expression_api},

    {"ms_logical_parse", "logical", "解析逻辑表达式。", demo_test_logical_api},
    {"ms_logical_tree_destroy", "logical", "释放逻辑树。", demo_test_logical_api},
    {"ms_logical_set", "logical", "设置 logical bit。", demo_test_logical_api},
    {"ms_logical_get", "logical", "读取 logical bit。", demo_test_logical_api},
    {"ms_logical_clear", "logical", "清空 logical vector。", demo_test_logical_api},
    {"ms_logical_eval", "logical", "计算解析后的逻辑树。", demo_test_logical_api},
    {"ms_logical_eval_inplace", "logical", "使用直接 key 树原地求值。", demo_test_logical_api},

    {"ms_set_allocator", "allocator", "一次配置全部 allocator 并验证对象分配。", demo_test_allocator_api},
    {"ms_set_database_allocator", "allocator", "定点验证 database 分配器。", demo_test_allocator_api},
    {"ms_set_scratch_allocator", "allocator", "定点验证 scratch 分配器。", demo_test_allocator_api},
    {"ms_set_stream_allocator", "allocator", "定点验证 stream 分配器。", demo_test_allocator_api},
    {"ms_set_misc_allocator", "allocator", "验证 misc setter 和默认恢复。", demo_test_allocator_api}
};

static int same(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

static const char *demo_api_ret_name(int ret)
{
    switch (ret) {
        case MS_SUCCESS:
            return "MS_SUCCESS";
        case MS_INVALID_PARA:
            return "MS_INVALID_PARA";
        case MS_MALLOC_FAIL:
            return "MS_MALLOC_FAIL";
        case MS_BUILT_ALREADY:
            return "MS_BUILT_ALREADY";
        case MS_FIND_FAIL:
            return "MS_FIND_FAIL";
        case MS_INIT_RWLOCK_FAIL:
            return "MS_INIT_RWLOCK_FAIL";
        case MS_MEMCPY_FAIL:
            return "MS_MEMCPY_FAIL";
        case MS_NOT_SUPPORTED:
            return "MS_NOT_SUPPORTED";
        case MS_STREAM_STALE:
            return "MS_STREAM_STALE";
        default:
            return "UNKNOWN";
    }
}

static void demo_cli_print_ret(const char *api, int ret)
{
    printf("调用 %-34s 返回=%d (%s)\n", api, ret, demo_api_ret_name(ret));
}

static int demo_check_ret(const char *phase, const char *name,
                          int actual, int expected)
{
    int pass = actual == expected;

    printf("[%s] %-34s actual=%d expected=%d %s\n",
           phase, name, actual, expected, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

static int demo_check_true(const char *phase, const char *name, int condition)
{
    printf("[%s] %-34s %s\n", phase, name, condition ? "PASS" : "FAIL");
    return condition ? 0 : 1;
}

static int demo_ids_contain(const uint32_t *ids, uint32_t count, uint32_t id)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        if (ids[i] == id) {
            return 1;
        }
    }
    return 0;
}

static int demo_compile_combo_db(ms_database_t **db)
{
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};

    return ms_compile_database_multi(db, MS_MATCHER_TYPE_AC, 0, 0,
                                     expr, flags, ids, 3);
}

static int demo_compile_plain_db(ms_database_t **db)
{
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};

    return ms_compile_database_multi(db, MS_MATCHER_TYPE_AC, 0, 0,
                                     expr, NULL, ids, 1);
}

static int demo_test_database_api(const char *api)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t out[4] = {0};
    uint32_t count = 0;
    int ret;
    int failed = 0;

    if (same(api, "ms_create_database")) {
        ret = ms_create_database(&db, MS_MATCHER_TYPE_AC, 0, 0);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "database 输出非空", db != NULL);
        if (db != NULL) (void)ms_free_database(db);
        return failed;
    }
    if (same(api, "ms_compile_database_multi")) {
        ret = demo_compile_plain_db(&db);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "编译结果非空", db != NULL);
        if (db != NULL) (void)ms_free_database(db);
        return failed;
    }
    if (same(api, "ms_compile_database_multi_ext")) {
        const char *expr[] = {"abc", "!101"};
        uint32_t ids[] = {101, 1001};
        uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_COMBINATION};
        ms_expr_ext_t ext[] = {{0, 0, 0}, {MS_EXT_FLAG_MAX_OFFSET, 0, 8}};

        ret = ms_compile_database_multi_ext(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                            expr, flags, ids, ext, 2);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "扩展编译结果非空", db != NULL);
        if (db != NULL) (void)ms_free_database(db);
        return failed;
    }
    if (same(api, "ms_free_database")) {
        ret = demo_compile_plain_db(&db);
        failed += demo_check_ret("SETUP", "编译 database", ret, MS_SUCCESS);
        if (ret == MS_SUCCESS) {
            ret = ms_free_database(db);
            db = NULL;
            failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        }
        return failed;
    }

    ret = (same(api, "ms_flush_stream")) ? MS_SUCCESS : demo_compile_combo_db(&db);
    if (same(api, "ms_flush_stream")) {
        const char *expr[] = {"abc", "!101"};
        uint32_t ids[] = {101, 2001};
        uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_COMBINATION};
        ret = ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                        expr, flags, ids, 2);
    }
    failed += demo_check_ret("SETUP", "编译测试 database", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) return failed;

    if (same(api, "ms_find_database_rule")) {
        ms_rule_t query;
        const char *expr[] = {"malware-demo"};
        uint32_t ids[] = {4001};

        (void)ms_free_database(db);
        db = NULL;
        ret = ms_compile_database_multi(&db, MS_MATCHER_TYPE_HASH, 16, 4,
                                        expr, NULL, ids, 1);
        failed += demo_check_ret("SETUP", "编译 HASH database", ret, MS_SUCCESS);
        query = demo_make_rule(0, "malware-demo");
        ret = ms_find_database_rule(db, &query);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "查询 alias == 4001", query.alias == 4001);
        (void)ms_free_database(db);
        return failed;
    }

    if (same(api, "ms_alloc_scratch")) {
        ret = ms_alloc_scratch(db, &scratch);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "scratch 输出非空", scratch != NULL);
        if (scratch != NULL) (void)ms_free_scratch(scratch);
        (void)ms_free_database(db);
        return failed;
    }

    ret = ms_alloc_scratch(db, &scratch);
    failed += demo_check_ret("SETUP", "分配 scratch", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) {
        (void)ms_free_database(db);
        return failed;
    }
    if (same(api, "ms_free_scratch")) {
        ret = ms_free_scratch(scratch);
        scratch = NULL;
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        (void)ms_free_database(db);
        return failed;
    }
    if (same(api, "ms_scan_database")) {
        ret = ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                               scratch, out, &count, 4);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "只报告组合 ID 1001",
                                  count == 1 && out[0] == 1001);
        (void)ms_free_scratch(scratch);
        (void)ms_free_database(db);
        return failed;
    }

    ret = ms_open_stream(db, &stream);
    if (same(api, "ms_open_stream")) {
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "stream 输出非空", stream != NULL);
    } else {
        failed += demo_check_ret("SETUP", "打开 stream", ret, MS_SUCCESS);
    }
    if (ret != MS_SUCCESS) goto cleanup;

    if (same(api, "ms_scan_stream")) {
        ret = ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                             scratch, out, &count, 4);
        failed += demo_check_ret("SETUP", "扫描第一个 chunk", ret, MS_FIND_FAIL);
        count = 0;
        ret = ms_scan_stream(stream, (const uint8_t *)"def", 3,
                             scratch, out, &count, 4);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "跨 chunk 报告组合 1001",
                                  count == 1 && out[0] == 1001);
    } else if (same(api, "ms_flush_stream")) {
        ret = ms_flush_stream(stream, scratch, out, &count, 4);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "flush 报告纯负组合 2001",
                                  count == 1 && out[0] == 2001);
    } else if (same(api, "ms_reset_stream")) {
        ret = ms_scan_stream(stream, (const uint8_t *)"ab", 2,
                             scratch, out, &count, 4);
        failed += demo_check_ret("SETUP", "制造半匹配状态", ret, MS_FIND_FAIL);
        ret = ms_reset_stream(stream);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        count = 0;
        ret = ms_scan_stream(stream, (const uint8_t *)"c", 1,
                             scratch, out, &count, 4);
        failed += demo_check_ret("VERIFY", "reset 后 c 不延续 ab", ret, MS_FIND_FAIL);
    } else if (same(api, "ms_close_stream")) {
        ret = ms_close_stream(stream, scratch, out, &count, 4);
        stream = NULL;
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
    }

cleanup:
    if (stream != NULL) {
        count = 0;
        (void)ms_close_stream(stream, scratch, out, &count, 4);
    }
    if (scratch != NULL) (void)ms_free_scratch(scratch);
    if (db != NULL) (void)ms_free_database(db);
    return failed;
}

static uint32_t demo_test_hash(const uint8_t *key, uint32_t key_len)
{
    uint32_t value = 2166136261u;
    uint32_t i;

    for (i = 0; i < key_len; i++) {
        value = (value ^ key[i]) * 16777619u;
    }
    return value;
}

static int demo_test_handler_api(const char *api)
{
    ms_handler_t handler = {0};
    ms_rule_t add = demo_make_rule(7001, "blocked-key");
    ms_rule_t query = demo_make_rule(0, "blocked-key");
    int ret;
    int failed = 0;

    ret = ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 16, 4);
    failed += demo_check_ret(same(api, "ms_handler_init") ? "TEST" : "SETUP",
                             "ms_handler_init", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) return failed;
    if (same(api, "ms_handler_init")) goto cleanup;
    if (same(api, "ms_handler_destroy")) goto cleanup;

    if (same(api, "ms_set_hash_func")) {
        ret = ms_set_hash_func(&handler, demo_test_hash);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        goto cleanup;
    }
    if (same(api, "ms_add_rule")) {
        ret = ms_add_rule(&handler, &add, 1);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        goto cleanup;
    }

    ret = ms_add_rule(&handler, &add, 1);
    failed += demo_check_ret("SETUP", "ms_add_rule", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) goto cleanup;
    if (same(api, "ms_find_rule")) {
        ret = ms_find_rule(&handler, &query);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "查询 alias == 7001", query.alias == 7001);
    } else if (same(api, "ms_remove_rule")) {
        ret = ms_remove_rule(&handler, &add, 1);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        query.alias = 0;
        ret = ms_find_rule(&handler, &query);
        failed += demo_check_ret("VERIFY", "删除后查询", ret, MS_FIND_FAIL);
    }

cleanup:
    if (same(api, "ms_handler_destroy")) {
        ret = ms_handler_destroy(&handler);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
    } else {
        (void)ms_handler_destroy(&handler);
    }
    return failed;
}

static int demo_prepare_expression_handler(ms_handler_t *handler, int pure_negative)
{
    const char *combo_expr[] = {"abc", "def", "101&102"};
    uint32_t combo_ids[] = {101, 102, 1001};
    uint32_t combo_flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    const char *negative_expr[] = {"abc", "!101"};
    uint32_t negative_ids[] = {101, 2001};
    uint32_t negative_flags[] = {MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    int ret;

    ret = ms_handler_init(handler, MS_MATCHER_TYPE_AC, 0, 0);
    if (ret != MS_SUCCESS) return ret;
    return pure_negative ?
           ms_compile_multi(handler, negative_expr, negative_flags, negative_ids, 2) :
           ms_compile_multi(handler, combo_expr, combo_flags, combo_ids, 3);
}

static int demo_test_expression_api(const char *api)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    uint32_t out[4] = {0};
    uint32_t count = 0;
    int ret;
    int failed = 0;
    int pure_negative = same(api, "ms_scan_expr_stream_flush");

    if (same(api, "ms_compile_multi") || same(api, "ms_compile_multi_ext") ||
        same(api, "ms_add_exprs")) {
        const char *expr[] = {"abc", "def", "101&102"};
        uint32_t ids[] = {101, 102, 1001};
        uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
        ms_expr_ext_t ext[] = {{0, 0, 0}, {0, 0, 0}, {MS_EXT_FLAG_MAX_OFFSET, 0, 16}};

        ret = ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0);
        failed += demo_check_ret("SETUP", "ms_handler_init", ret, MS_SUCCESS);
        if (ret != MS_SUCCESS) return failed;
        if (same(api, "ms_compile_multi")) {
            ret = ms_compile_multi(&handler, expr, flags, ids, 3);
        } else if (same(api, "ms_compile_multi_ext")) {
            ret = ms_compile_multi_ext(&handler, expr, flags, ids, ext, 3);
        } else {
            ret = ms_add_exprs(&handler, expr, ids, flags, 3);
        }
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        (void)ms_handler_destroy(&handler);
        return failed;
    }

    ret = demo_prepare_expression_handler(&handler, pure_negative);
    failed += demo_check_ret("SETUP", "准备表达式 handler", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) {
        (void)ms_handler_destroy(&handler);
        return failed;
    }
    if (same(api, "ms_scan_expr")) {
        ret = ms_scan_expr(&handler, (const uint8_t *)"abcdef", 6, out, &count, 4);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "报告组合 1001",
                                  count == 1 && out[0] == 1001);
        goto cleanup;
    }
    if (same(api, "ms_compile_eval_combinations")) {
        uint32_t aliases[] = {101, 102};
        ret = ms_compile_eval_combinations(&handler, aliases, 2, out, &count, 4);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "alias 求值报告 1001",
                                  count == 1 && out[0] == 1001);
        goto cleanup;
    }
    if (same(api, "ms_compile_scan_ac_combinations")) {
        ret = ms_compile_scan_ac_combinations(&handler, (const uint8_t *)"abcdef", 6,
                                              out, &count, 4);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "扫描只报告组合 1001",
                                  count == 1 && out[0] == 1001);
        goto cleanup;
    }

    ret = ms_open_handler_stream(&handler, &stream);
    failed += demo_check_ret(same(api, "ms_open_handler_stream") ? "TEST" : "SETUP",
                             "ms_open_handler_stream", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) goto cleanup;
    if (same(api, "ms_open_handler_stream")) goto cleanup;
    if (same(api, "ms_scan_expr_stream")) {
        ret = ms_scan_expr_stream(stream, (const uint8_t *)"abcdef", 6, out, &count, 4);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "Stream 报告组合 1001",
                                  count == 1 && out[0] == 1001);
    } else if (same(api, "ms_scan_rule_stream")) {
        ret = ms_scan_rule_stream(stream, (const uint8_t *)"abcdef", 6);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
    } else if (same(api, "ms_scan_expr_stream_flush")) {
        ret = ms_scan_expr_stream_flush(stream, out, &count, 4);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "flush 报告纯负组合 2001",
                                  count == 1 && out[0] == 2001);
    } else if (same(api, "ms_close_handler_stream")) {
        ret = ms_close_handler_stream(stream);
        stream = NULL;
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
    }

cleanup:
    if (stream != NULL) (void)ms_close_handler_stream(stream);
    (void)ms_handler_destroy(&handler);
    return failed;
}

static int demo_test_logical_api(const char *api)
{
    ms_logical_tree_t tree = {0};
    uint8_t vector[32] = {0};
    uint8_t matched = 0;
    uint8_t value = 0;
    int ret;
    int failed = 0;

    if (same(api, "ms_logical_set")) {
        ret = ms_logical_set(vector, 256, 101, 1);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "bit 101 已设置", (vector[12] & 0x20u) != 0);
        return failed;
    }
    (void)ms_logical_set(vector, 256, 101, 1);
    if (same(api, "ms_logical_get")) {
        ret = ms_logical_get(vector, 256, 101, &value);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "读取值为 1", value == 1);
        return failed;
    }
    if (same(api, "ms_logical_clear")) {
        ret = ms_logical_clear(vector, 256);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        (void)ms_logical_get(vector, 256, 101, &value);
        failed += demo_check_true("VERIFY", "清空后值为 0", value == 0);
        return failed;
    }
    if (same(api, "ms_logical_eval_inplace")) {
        ms_logical_op_t op = {200, MS_LOGICAL_OP_AND, 101, 102};
        ms_logical_tree_t direct = {&op, 1, 1, 200};

        (void)ms_logical_set(vector, 256, 102, 1);
        ret = ms_logical_eval_inplace(&direct, vector, 256, &matched);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "原地求值结果为 1", matched == 1);
        return failed;
    }

    ret = ms_logical_parse("101&!102", &tree);
    failed += demo_check_ret(same(api, "ms_logical_parse") ? "TEST" : "SETUP",
                             "ms_logical_parse", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) return failed;
    if (same(api, "ms_logical_eval")) {
        ret = ms_logical_eval(&tree, vector, 256, &matched);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        failed += demo_check_true("VERIFY", "101=1,102=0 时成立", matched == 1);
    }
    if (same(api, "ms_logical_tree_destroy")) {
        ms_logical_tree_destroy(&tree);
        failed += demo_check_true("TEST", "ms_logical_tree_destroy",
                                  tree.ops == NULL && tree.op_count == 0);
        return failed;
    }
    ms_logical_tree_destroy(&tree);
    return failed;
}

static size_t g_test_allocs;
static size_t g_test_callocs;
static size_t g_test_frees;

static void *MS_CDECL demo_test_alloc(size_t size)
{
    g_test_allocs++;
    return malloc(size);
}

static void *MS_CDECL demo_test_calloc(size_t nmemb, size_t size)
{
    g_test_callocs++;
    return calloc(nmemb, size);
}

static void MS_CDECL demo_test_free(void *ptr)
{
    g_test_frees++;
    free(ptr);
}

static void demo_reset_allocator_counts(void)
{
    g_test_allocs = 0;
    g_test_callocs = 0;
    g_test_frees = 0;
}

static void demo_restore_allocators(void)
{
    (void)ms_set_allocator(NULL, NULL, NULL);
}

static int demo_test_allocator_api(const char *api)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t out[1] = {0};
    uint32_t count = 0;
    int ret;
    int failed = 0;

    demo_restore_allocators();
    demo_reset_allocator_counts();

    if (same(api, "ms_set_database_allocator")) {
        ret = ms_set_database_allocator(demo_test_alloc, demo_test_calloc, demo_test_free);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        ret = demo_compile_plain_db(&db);
        failed += demo_check_ret("VERIFY", "使用 database allocator 编译", ret, MS_SUCCESS);
        if (db != NULL) (void)ms_free_database(db);
        failed += demo_check_true("VERIFY", "database allocator 被调用",
                                  g_test_allocs + g_test_callocs > 0 && g_test_frees > 0);
        (void)ms_set_database_allocator(NULL, NULL, NULL);
        return failed;
    }

    ret = demo_compile_plain_db(&db);
    failed += demo_check_ret("SETUP", "默认 allocator 编译 database", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) return failed;

    if (same(api, "ms_set_scratch_allocator")) {
        ret = ms_set_scratch_allocator(demo_test_alloc, demo_test_calloc, demo_test_free);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        ret = ms_alloc_scratch(db, &scratch);
        failed += demo_check_ret("VERIFY", "使用 scratch allocator 分配", ret, MS_SUCCESS);
        if (scratch != NULL) (void)ms_free_scratch(scratch);
        failed += demo_check_true("VERIFY", "scratch allocator 被调用",
                                  g_test_allocs + g_test_callocs > 0 && g_test_frees > 0);
        (void)ms_set_scratch_allocator(NULL, NULL, NULL);
    } else if (same(api, "ms_set_stream_allocator")) {
        ret = ms_set_stream_allocator(demo_test_alloc, demo_test_calloc, demo_test_free);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        ret = ms_open_stream(db, &stream);
        failed += demo_check_ret("VERIFY", "使用 stream allocator 打开", ret, MS_SUCCESS);
        if (stream != NULL) (void)ms_close_handler_stream(stream);
        failed += demo_check_true("VERIFY", "stream allocator 被调用",
                                  g_test_allocs + g_test_callocs > 0 && g_test_frees > 0);
        (void)ms_set_stream_allocator(NULL, NULL, NULL);
    } else if (same(api, "ms_set_misc_allocator")) {
        ret = ms_set_misc_allocator(demo_test_alloc, demo_test_calloc, demo_test_free);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        demo_note("验证范围", "当前没有使用 misc allocator 的公开对象创建接口，仅验证 setter 与恢复。" );
        (void)ms_set_misc_allocator(NULL, NULL, NULL);
    } else if (same(api, "ms_set_allocator")) {
        (void)ms_free_database(db);
        db = NULL;
        ret = ms_set_allocator(demo_test_alloc, demo_test_calloc, demo_test_free);
        failed += demo_check_ret("TEST", api, ret, MS_SUCCESS);
        ret = demo_compile_plain_db(&db);
        failed += demo_check_ret("VERIFY", "编译 database", ret, MS_SUCCESS);
        if (ret == MS_SUCCESS) ret = ms_alloc_scratch(db, &scratch);
        failed += demo_check_ret("VERIFY", "分配 scratch", ret, MS_SUCCESS);
        if (ret == MS_SUCCESS) ret = ms_open_stream(db, &stream);
        failed += demo_check_ret("VERIFY", "打开 stream", ret, MS_SUCCESS);
        if (stream != NULL) {
            (void)ms_close_stream(stream, scratch, out, &count, 1);
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
        failed += demo_check_true("VERIFY", "统一 allocator 被调用并释放",
                                  g_test_allocs + g_test_callocs > 0 && g_test_frees > 0);
        demo_restore_allocators();
        return failed;
    }

    if (db != NULL) (void)ms_free_database(db);
    demo_restore_allocators();
    return failed;
}

static int demo_test_database_remove(const char *api)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_NONE, MS_FLAG_COMBINATION};
    ms_rule_t remove_combo = {1001, 0, NULL};
    ms_rule_t remove_plain = {101, 3, (uint8_t *)"abc"};
    ms_rule_t failed_batch[] = {
        {101, 3, (uint8_t *)"abc"},
        {102, 3, (uint8_t *)"xyz"}
    };
    uint32_t out[4] = {0};
    uint32_t count = 0;
    int ret;
    int failed = 0;

    (void)api;
    ret = ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                    expr, flags, ids, 3);
    failed += demo_check_ret("SETUP", "编译普通+组合规则", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) return failed;
    ret = ms_alloc_scratch(db, &scratch);
    failed += demo_check_ret("SETUP", "分配 scratch", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) goto cleanup;
    ret = ms_open_stream(db, &stream);
    failed += demo_check_ret("SETUP", "删除前打开 stream", ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) goto cleanup;

    ret = ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                           scratch, out, &count, 4);
    failed += demo_check_ret("VERIFY", "删除前 Block 扫描", ret, MS_SUCCESS);
    failed += demo_check_true("VERIFY", "删除前包含 101/102/1001",
                              count == 3 && demo_ids_contain(out, count, 101) &&
                              demo_ids_contain(out, count, 102) &&
                              demo_ids_contain(out, count, 1001));

    ret = ms_remove_database_rule(db, failed_batch, 2);
    failed += demo_check_ret("TEST", "批量删除中一条不匹配", ret, MS_FIND_FAIL);
    count = 0;
    ret = ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                           scratch, out, &count, 4);
    failed += demo_check_ret("VERIFY", "失败后 database 仍可扫描", ret, MS_SUCCESS);
    failed += demo_check_true("VERIFY", "失败批次整批未提交",
                              count == 3 && demo_ids_contain(out, count, 101) &&
                              demo_ids_contain(out, count, 102) &&
                              demo_ids_contain(out, count, 1001));

    ret = ms_remove_database_rule(db, &remove_combo, 1);
    failed += demo_check_ret("TEST", "定点删除组合规则 1001", ret, MS_SUCCESS);
    count = 0;
    ret = ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                           scratch, out, &count, 4);
    failed += demo_check_ret("VERIFY", "复用旧 scratch 扫描", ret, MS_SUCCESS);
    failed += demo_check_true("VERIFY", "组合消失且普通规则保留",
                              count == 2 && demo_ids_contain(out, count, 101) &&
                              demo_ids_contain(out, count, 102) &&
                              !demo_ids_contain(out, count, 1001));

    count = 0;
    ret = ms_scan_stream(stream, (const uint8_t *)"abcdef", 6,
                         scratch, out, &count, 4);
    failed += demo_check_ret("VERIFY", "旧 stream 检测 generation", ret, MS_STREAM_STALE);
    ret = ms_reset_stream(stream);
    failed += demo_check_ret("VERIFY", "reset 重绑定新规则集", ret, MS_SUCCESS);

    ret = ms_remove_database_rule(db, &remove_plain, 1);
    failed += demo_check_ret("TEST", "定点删除普通规则 101", ret, MS_SUCCESS);
    count = 0;
    ret = ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                           scratch, out, &count, 4);
    failed += demo_check_ret("VERIFY", "普通规则删除后扫描", ret, MS_SUCCESS);
    failed += demo_check_true("VERIFY", "只剩普通规则 102",
                              count == 1 && out[0] == 102);
    count = 0;
    ret = ms_scan_stream(stream, (const uint8_t *)"def", 3,
                         scratch, out, &count, 4);
    failed += demo_check_ret("VERIFY", "第二次删除后 stream stale", ret, MS_STREAM_STALE);
    ret = ms_reset_stream(stream);
    failed += demo_check_ret("VERIFY", "第二次 reset", ret, MS_SUCCESS);

cleanup:
    if (stream != NULL) {
        count = 0;
        (void)ms_close_stream(stream, scratch, out, &count, 4);
    }
    if (scratch != NULL) (void)ms_free_scratch(scratch);
    if (db != NULL) (void)ms_free_database(db);
    return failed;
}

static const demo_api_test_t *demo_find_api_test(const char *api)
{
    uint32_t i;

    for (i = 0; i < DEMO_ARRAY_SIZE(g_api_tests); i++) {
        if (same(g_api_tests[i].name, api)) return &g_api_tests[i];
    }
    return NULL;
}

static const demo_module_t *demo_find_module(const char *module)
{
    uint32_t i;

    for (i = 0; i < DEMO_ARRAY_SIZE(g_modules); i++) {
        if (same(g_modules[i].key, module)) return &g_modules[i];
    }
    return NULL;
}

void demo_list_api_tests(void)
{
    uint32_t i;

    printf("可单独执行的公开接口测试：\n");
    for (i = 0; i < DEMO_ARRAY_SIZE(g_api_tests); i++) {
        printf("  %-36s [%-10s] %s\n", g_api_tests[i].name,
               g_api_tests[i].module, g_api_tests[i].summary);
    }
    printf("\n运行：megascan_demo --api <接口名>\n");
}

void demo_print_api_cli_help(const char *prog)
{
    printf("用法：\n");
    printf("  %s --api <接口名>\n", prog);
    printf("      运行该接口的自动测试。\n");
    printf("  %s --api <接口名> [参数...]\n", prog);
    printf("      使用命令行参数调用该接口。\n");
    printf("  %s --api <接口名> -h\n", prog);
    printf("      查看该接口需要的参数、参数含义和示例。\n\n");
    printf("当前支持 CLI 参数调用的接口：\n");
    printf("  ms_compile_database_multi <hash|ac> <buckets> <secondary_buckets> "
           "<expr id flags>...\n");
    printf("  ms_alloc_scratch           <hash|ac> <buckets> <secondary_buckets> "
           "<expr id flags>...\n");
    printf("  ms_free_scratch            <hash|ac> <buckets> <secondary_buckets> "
           "<expr id flags>...\n");
    printf("  ms_open_stream             <hash|ac> <buckets> <secondary_buckets> "
           "<expr id flags>...\n");
    printf("  ms_scan_database           <data> <id_capacity> <hash|ac> <buckets> "
           "<secondary_buckets> <expr id flags>...\n");
    printf("  ms_remove_database_rule    <alias> <key|-> <key_len> <hash|ac> "
           "<buckets> <secondary_buckets> <expr id flags>...\n\n");
    printf("规则按三元组传入：<expr id flags>。\n");
    printf("flags: 0=MS_FLAG_NONE, 1=MS_FLAG_COMBINATION, 2=MS_FLAG_QUIET, "
           "3=COMBINATION|QUIET\n");
}

void demo_print_api_test_help(const char *prog, const char *api)
{
    const demo_api_test_t *test = demo_find_api_test(api);

    if (test == NULL) {
        printf("未知或尚未注册接口：%s\n\n", api);
        demo_list_api_tests();
        return;
    }
    printf("接口：%s\n模块：%s\n测试：%s\n", test->name, test->module, test->summary);
    printf("\n自动测试：\n");
    printf("  %s --api %s\n", prog, test->name);
    printf("输出中 SETUP/CLEANUP 是依赖调用，TEST 是目标接口，VERIFY 是结果断言。\n");

    if (same(api, "ms_compile_database_multi") || same(api, "ms_alloc_scratch") ||
        same(api, "ms_free_scratch") || same(api, "ms_open_stream")) {
        printf("\nCLI 参数调用：\n");
        printf("  %s --api %s <hash|ac> <buckets> <secondary_buckets> "
               "<expr id flags>...\n", prog, api);
        printf("参数：\n");
        printf("  hash|ac            database matcher 类型\n");
        printf("  buckets            HASH 主桶数量；AC 可传 0\n");
        printf("  secondary_buckets  HASH 次级桶数量；AC 可传 0\n");
        printf("  expr id flags      一条规则，可重复传入多组\n");
        printf("  flags              0=NONE, 1=COMBINATION, 2=QUIET, 3=COMBINATION|QUIET\n");
        if (same(api, "ms_open_stream")) {
            printf("限制：ms_open_stream 只支持 AC database。\n");
        }
        printf("示例：\n");
        printf("  %s --api %s ac 0 0 abc 101 0 def 102 0 '101&102' 1001 1\n",
               prog, api);
        return;
    }

    if (same(api, "ms_scan_database")) {
        printf("\nCLI 参数调用：\n");
        printf("  %s --api ms_scan_database <data> <id_capacity> <hash|ac> "
               "<buckets> <secondary_buckets> <expr id flags>...\n", prog);
        printf("参数：\n");
        printf("  data               要扫描的命令行文本\n");
        printf("  id_capacity        输出 ID 数组容量\n");
        printf("  其余参数           用于构造 database，规则按 expr/id/flags 三元组传入\n");
        printf("限制：ms_scan_database 只支持 AC database。\n");
        printf("示例：\n");
        printf("  %s --api ms_scan_database abcdef 4 ac 0 0 "
               "abc 101 0 def 102 0 '101&102' 1001 1\n", prog);
        return;
    }

    if (same(api, "ms_remove_database_rule")) {
        printf("\nCLI 参数调用：\n");
        printf("  %s --api ms_remove_database_rule <alias> <key|-> <key_len> "
               "ac 0 0 <expr id flags>...\n", prog);
        printf("参数：\n");
        printf("  alias              要删除规则的 ID\n");
        printf("  key                普通规则原文；组合规则传 -，表示 NULL\n");
        printf("  key_len            普通规则 key 的字节数；组合规则传 0\n");
        printf("  ac 0 0             构造 compact AC database\n");
        printf("  expr id flags      删除前 database 中的规则，可重复传入多组\n");
        printf("限制：当前 CLI 每次调用 rule_num=1；接口只支持 compact AC database。\n");
        printf("普通规则示例：\n");
        printf("  %s --api ms_remove_database_rule 101 abc 3 ac 0 0 "
               "abc 101 0 def 102 0 '101&102' 1001 1\n", prog);
        printf("组合规则示例：\n");
        printf("  %s --api ms_remove_database_rule 1001 - 0 ac 0 0 "
               "abc 101 0 def 102 0 '101&102' 1001 1\n", prog);
        return;
    }

    printf("\n该接口目前只支持无参数自动测试，尚未支持 CLI 参数调用。\n");
}

int demo_run_api_test(const char *api)
{
    const demo_api_test_t *test = demo_find_api_test(api);
    int failed;

    if (test == NULL) {
        printf("未知或尚未注册接口：%s\n\n", api);
        demo_list_api_tests();
        return 1;
    }
    printf("\n== 单接口测试：%s [%s] ==\n%s\n", test->name, test->module, test->summary);
    failed = test->fn(test->name);
    printf("[RESULT] %s %s\n", test->name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

static int demo_cli_parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int demo_cli_parse_matcher(const char *text, ms_matcher_type *type)
{
    if (same(text, "hash") || same(text, "HASH") || same(text, "1")) {
        *type = MS_MATCHER_TYPE_HASH;
        return 0;
    }
    if (same(text, "ac") || same(text, "AC") || same(text, "2")) {
        *type = MS_MATCHER_TYPE_AC;
        return 0;
    }
    return -1;
}

static void demo_cli_print_db_usage(const char *prog, const char *api)
{
    printf("用法：%s --api %s <hash|ac> <buckets> <secondary_buckets> "
           "<expr id flags>...\n", prog, api);
    printf("flags: 0=MS_FLAG_NONE, 1=MS_FLAG_COMBINATION, 2=MS_FLAG_QUIET, "
           "3=COMBINATION|QUIET\n");
    printf("示例：%s --api %s ac 0 0 abc 101 0 def 102 0 '101&102' 1001 1\n",
           prog, api);
}

static int demo_cli_parse_db_spec(int argc, char **argv,
                                  ms_matcher_type *type,
                                  uint32_t *buckets,
                                  uint32_t *secondary_buckets,
                                  const char ***expressions,
                                  uint32_t **ids,
                                  uint32_t **flags,
                                  uint32_t *elements)
{
    uint32_t count;
    uint32_t i;
    const char **expr = NULL;
    uint32_t *rule_ids = NULL;
    uint32_t *rule_flags = NULL;

    if (argc < 6 || ((argc - 3) % 3) != 0 ||
        demo_cli_parse_matcher(argv[0], type) != 0 ||
        demo_cli_parse_u32(argv[1], buckets) != 0 ||
        demo_cli_parse_u32(argv[2], secondary_buckets) != 0) {
        return -1;
    }
    count = (uint32_t)((argc - 3) / 3);
    expr = (const char **)calloc((size_t)count + 1u, sizeof(*expr));
    rule_ids = (uint32_t *)calloc(count, sizeof(*rule_ids));
    rule_flags = (uint32_t *)calloc(count, sizeof(*rule_flags));
    if (expr == NULL || rule_ids == NULL || rule_flags == NULL) {
        free(expr);
        free(rule_ids);
        free(rule_flags);
        return -1;
    }
    for (i = 0; i < count; i++) {
        expr[i] = argv[3 + i * 3];
        if (demo_cli_parse_u32(argv[4 + i * 3], &rule_ids[i]) != 0 ||
            demo_cli_parse_u32(argv[5 + i * 3], &rule_flags[i]) != 0) {
            free(expr);
            free(rule_ids);
            free(rule_flags);
            return -1;
        }
    }
    *expressions = expr;
    *ids = rule_ids;
    *flags = rule_flags;
    *elements = count;
    return 0;
}

static void demo_cli_free_db_spec(const char **expressions,
                                  uint32_t *ids,
                                  uint32_t *flags)
{
    free((void *)expressions);
    free(ids);
    free(flags);
}

static int demo_cli_compile_db(int argc, char **argv, ms_database_t **db)
{
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary_buckets;
    const char **expressions = NULL;
    uint32_t *ids = NULL;
    uint32_t *flags = NULL;
    uint32_t elements = 0;
    int ret;

    if (demo_cli_parse_db_spec(argc, argv, &type, &buckets, &secondary_buckets,
                               &expressions, &ids, &flags, &elements) != 0) {
        return MS_INVALID_PARA;
    }
    printf("实际调用：ms_compile_database_multi(&db, type=%u, buckets=%u, "
           "secondary=%u, expressions, flags, ids, elements=%u)\n",
           (uint32_t)type, buckets, secondary_buckets, elements);
    ret = ms_compile_database_multi(db, type, buckets, secondary_buckets,
                                    expressions, flags, ids, elements);
    demo_cli_free_db_spec(expressions, ids, flags);
    return ret;
}

int demo_run_api_cli(const char *api, int argc, char **argv)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    uint32_t out[4] = {0};
    uint32_t count = 0;
    int ret = MS_INVALID_PARA;

    if (argc == 0) {
        return demo_run_api_test(api);
    }

    if (same(api, "ms_compile_database_multi")) {
        ret = demo_cli_compile_db(argc, argv, &db);
        demo_cli_print_ret("ms_compile_database_multi", ret);
        if (db != NULL) (void)ms_free_database(db);
        return ret == MS_SUCCESS ? 0 : 1;
    }

    if (same(api, "ms_alloc_scratch") || same(api, "ms_free_scratch") ||
        same(api, "ms_open_stream")) {
        ret = demo_cli_compile_db(argc, argv, &db);
        if (ret != MS_SUCCESS) {
            demo_cli_print_ret("SETUP ms_compile_database_multi", ret);
            return 1;
        }
        if (same(api, "ms_alloc_scratch")) {
            printf("实际调用：ms_alloc_scratch(db, &scratch)\n");
            ret = ms_alloc_scratch(db, &scratch);
            demo_cli_print_ret("ms_alloc_scratch", ret);
        } else if (same(api, "ms_free_scratch")) {
            printf("SETUP：ms_alloc_scratch(db, &scratch)\n");
            ret = ms_alloc_scratch(db, &scratch);
            demo_cli_print_ret("SETUP ms_alloc_scratch", ret);
            if (ret == MS_SUCCESS) {
                printf("实际调用：ms_free_scratch(scratch)\n");
                ret = ms_free_scratch(scratch);
                scratch = NULL;
                demo_cli_print_ret("ms_free_scratch", ret);
            }
        } else {
            printf("实际调用：ms_open_stream(db, &stream)\n");
            ret = ms_open_stream(db, &stream);
            demo_cli_print_ret("ms_open_stream", ret);
        }
        if (stream != NULL) {
            (void)ms_close_stream(stream, scratch, out, &count,
                                  (uint32_t)DEMO_ARRAY_SIZE(out));
        }
        if (scratch != NULL) (void)ms_free_scratch(scratch);
        if (db != NULL) (void)ms_free_database(db);
        return ret == MS_SUCCESS ? 0 : 1;
    }

    if (same(api, "ms_scan_database")) {
        uint32_t capacity;

        if (argc < 3 || demo_cli_parse_u32(argv[1], &capacity) != 0) {
            printf("用法：megascan_demo --api ms_scan_database <data> <id_capacity> "
                   "<hash|ac> <buckets> <secondary_buckets> <expr id flags>...\n");
            return 1;
        }
        ret = demo_cli_compile_db(argc - 2, argv + 2, &db);
        if (ret == MS_SUCCESS) ret = ms_alloc_scratch(db, &scratch);
        if (ret != MS_SUCCESS) {
            demo_cli_print_ret("SETUP", ret);
            if (db != NULL) (void)ms_free_database(db);
            return 1;
        }
        printf("实际调用：ms_scan_database(db, \"%s\", len=%u, scratch, out, "
               "&count, id_capacity=%u)\n",
               argv[0], (uint32_t)strlen(argv[0]), capacity);
        ret = ms_scan_database(db, (const uint8_t *)argv[0],
                               (uint32_t)strlen(argv[0]), scratch, out,
                               &count, capacity);
        demo_cli_print_ret("ms_scan_database", ret);
        printf("输出：id_count=%u\n", count);
        demo_print_ids("ids", out, count);
        (void)ms_free_scratch(scratch);
        (void)ms_free_database(db);
        return (ret == MS_SUCCESS || ret == MS_FIND_FAIL) ? 0 : 1;
    }

    if (same(api, "ms_remove_database_rule")) {
        ms_rule_t rule;

        if (argc < 9 || demo_cli_parse_u32(argv[0], &rule.alias) != 0 ||
            demo_cli_parse_u32(argv[2], &rule.key_len) != 0) {
            printf("用法：megascan_demo --api ms_remove_database_rule "
                   "<alias> <key|- for NULL> <key_len> "
                   "<hash|ac> <buckets> <secondary_buckets> <expr id flags>...\n");
            return 1;
        }
        rule.key = same(argv[1], "-") ? NULL : (uint8_t *)argv[1];
        ret = demo_cli_compile_db(argc - 3, argv + 3, &db);
        if (ret != MS_SUCCESS) {
            demo_cli_print_ret("SETUP ms_compile_database_multi", ret);
            return 1;
        }
        printf("实际调用：ms_remove_database_rule(db, {alias=%u,key=%s,key_len=%u}, 1)\n",
               rule.alias, rule.key == NULL ? "NULL" : (char *)rule.key,
               rule.key_len);
        ret = ms_remove_database_rule(db, &rule, 1);
        demo_cli_print_ret("ms_remove_database_rule", ret);
        (void)ms_free_database(db);
        return ret == MS_SUCCESS ? 0 : 1;
    }

    printf("接口 %s 尚未支持 CLI 参数化调用；无参数 --api 仍可跑自动测试。\n", api);
    return 1;
}

int demo_run_all_api_tests(void)
{
    uint32_t i;
    uint32_t failed = 0;

    for (i = 0; i < DEMO_ARRAY_SIZE(g_api_tests); i++) {
        if (demo_run_api_test(g_api_tests[i].name) != 0) failed++;
    }
    printf("\n== 接口测试汇总 ==\n总数=%u 通过=%u 失败=%u\n",
           (uint32_t)DEMO_ARRAY_SIZE(g_api_tests),
           (uint32_t)DEMO_ARRAY_SIZE(g_api_tests) - failed, failed);
    return failed == 0 ? 0 : 1;
}

void demo_list_modules(void)
{
    uint32_t i;

    printf("接口模块：\n");
    for (i = 0; i < DEMO_ARRAY_SIZE(g_modules); i++) {
        printf("  %-12s %s\n", g_modules[i].key, g_modules[i].title);
    }
    printf("\n查看流程：megascan_demo --module <模块>\n");
    printf("测试模块：megascan_demo --module-test <模块>\n");
}

int demo_show_module(const char *module)
{
    const demo_module_t *item = demo_find_module(module);
    uint32_t i;

    if (item == NULL) {
        printf("未知模块：%s\n\n", module);
        demo_list_modules();
        return 1;
    }
    printf("\n== %s [%s] ==\n%s\n\n模块接口测试：\n",
           item->title, item->key, item->flow);
    for (i = 0; i < DEMO_ARRAY_SIZE(g_api_tests); i++) {
        if (same(g_api_tests[i].module, module)) {
            printf("  %s\n", g_api_tests[i].name);
        }
    }
    return 0;
}

int demo_run_module_tests(const char *module)
{
    uint32_t i;
    uint32_t count = 0;
    uint32_t failed = 0;

    if (demo_show_module(module) != 0) return 1;
    for (i = 0; i < DEMO_ARRAY_SIZE(g_api_tests); i++) {
        if (same(g_api_tests[i].module, module)) {
            count++;
            if (demo_run_api_test(g_api_tests[i].name) != 0) failed++;
        }
    }
    printf("\n[MODULE RESULT] %s 总数=%u 通过=%u 失败=%u\n",
           module, count, count - failed, failed);
    return failed == 0 ? 0 : 1;
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Public API catalog and unified CLI dispatch entry.
 */
#include "demo_common.h"
#include "api_cli_demo.h"

#include <stdio.h>

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
    {"database", "Database 编译与生命周期",
     "compile -> database -> free"},
    {"block", "AC Block 扫描与组合",
     "database -> scratch -> scan_database -> ids"},
    {"stream", "AC Stream 扫描",
     "database + scratch -> open -> scan/flush/reset/close"},
    {"delete", "动态删除",
     "remove_database_rule -> scratch refresh / stream stale"},
    {"handler", "Handler 兼容接口",
     "handler_init -> add/find/remove_rule -> handler_destroy"},
    {"expression", "组合表达式兼容接口",
     "compile_multi -> block/stream combination evaluation"},
    {"logical", "逻辑表达式",
     "logical_parse -> set/get/eval -> tree_destroy"},
    {"allocator", "分类内存分配",
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

static const demo_api_t *find_api(const char *name)
{
    uint32_t i;

    for (i = 0; i < DEMO_ARRAY_SIZE(g_apis); i++) {
        if (demo_same(g_apis[i].name, name)) return &g_apis[i];
    }
    return NULL;
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
    (void)demo_print_api_command_help(prog, api);
}

int demo_run_api_cli(const char *api, int argc, char **argv)
{
    int ret;

    if (find_api(api) == NULL) {
        printf("unknown api: %s\n", api);
        return 1;
    }
    ret = demo_run_api_command(api, argc, argv);
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
        if (!demo_same(g_modules[i].key, module)) continue;
        printf("%s\nflow: %s\napis:", g_modules[i].title, g_modules[i].flow);
        for (j = 0; j < DEMO_ARRAY_SIZE(g_apis); j++) {
            if (demo_same(g_apis[j].module, module)) {
                printf(" %s", g_apis[j].name);
            }
        }
        printf("\n");
        return 0;
    }
    printf("unknown module: %s\n", module);
    return 1;
}

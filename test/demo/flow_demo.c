/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Copyable manual session flow templates.
 */
#include "flow_demo.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *summary;
    const char *script;
} demo_flow_t;

static const demo_flow_t g_flows[] = {
    {
        "block",
        "Database -> scratch -> Block scan -> explicit free",
        "# Expected: ids=1001 and commands_failed=0\n"
        "ms_compile_database_multi db1 ac 0 0 abc def '101&102' "
        "2 2 1 101 102 1001 3\n"
        "ms_alloc_scratch db1 scratch1\n"
        "ms_scan_database db1 abcdef 6 scratch1 8\n"
        "ms_free_scratch scratch1\n"
        "ms_free_database db1\n"
        "handles\n"
        "exit\n"
    },
    {
        "stream",
        "Database stream with two chunks, flush and close",
        "# Expected: second chunk reports id=101; commands_failed=0\n"
        "ms_compile_database_multi db1 ac 0 0 abcdef 0 101 1\n"
        "ms_alloc_scratch db1 scratch1\n"
        "ms_open_stream db1 stream1\n"
        "ms_scan_stream stream1 abc 3 scratch1 8\n"
        "ms_scan_stream stream1 def 3 scratch1 8\n"
        "ms_flush_stream stream1 scratch1 8\n"
        "ms_close_stream stream1 scratch1 8\n"
        "ms_free_scratch scratch1\n"
        "ms_free_database db1\n"
        "exit\n"
    },
    {
        "handler",
        "Mutable handler rules, expression scan and handler stream",
        "# Expected: block and second stream chunk report id=1001\n"
        "ms_handler_init handler1 ac 0 0\n"
        "ms_compile_multi handler1 abc def '101&102' "
        "2 2 1 101 102 1001 3\n"
        "ms_scan_expr handler1 abcdef 6 8\n"
        "ms_open_handler_stream handler1 hstream1\n"
        "ms_scan_expr_stream hstream1 abc 3 8\n"
        "ms_scan_expr_stream hstream1 def 3 8\n"
        "ms_scan_expr_stream_flush hstream1 8\n"
        "ms_close_handler_stream hstream1\n"
        "ms_handler_destroy handler1\n"
        "exit\n"
    },
    {
        "hash-handler",
        "Mutable HASH handler add/find/remove lifecycle",
        "# Expected: first find succeeds; remove succeeds\n"
        "ms_handler_init hash1 hash 16 4\n"
        "ms_set_hash_func hash1 custom\n"
        "ms_add_rule hash1 7001 7 blocked 1\n"
        "ms_find_rule hash1 7001 7 blocked\n"
        "ms_remove_rule hash1 7001 7 blocked 1\n"
        "ms_handler_destroy hash1\n"
        "exit\n"
    },
    {
        "logical",
        "Persistent logical tree and vector lifecycle",
        "# Expected: matched=1 and key 101 value=1\n"
        "ms_logical_parse tree1 '101&!102'\n"
        "logical_vector_create vector1 256\n"
        "ms_logical_set vector1 101 1\n"
        "ms_logical_set vector1 102 0\n"
        "ms_logical_get vector1 101\n"
        "ms_logical_eval tree1 vector1\n"
        "logical_tree_create_op tree2 200 or 101 102\n"
        "ms_logical_eval_inplace tree2 vector1\n"
        "ms_logical_clear vector1\n"
        "logical_vector_free vector1\n"
        "ms_logical_tree_destroy tree1\n"
        "ms_logical_tree_destroy tree2\n"
        "exit\n"
    },
    {
        "allocator",
        "Persistent allocator setting, real object use, stats and restore",
        "# Expected: allocator counters increase; active_mask returns to 0\n"
        "allocator_stats reset\n"
        "ms_set_database_allocator custom custom custom\n"
        "ms_compile_database_multi db1 ac 0 0 abc 0 101 1\n"
        "ms_free_database db1\n"
        "allocator_stats\n"
        "ms_set_database_allocator default default default\n"
        "allocator_stats\n"
        "exit\n"
    },
    {
        "delete",
        "Database rule deletion and stale stream recovery",
        "# Expected failure: old stream returns MS_STREAM_STALE\n"
        "ms_compile_database_multi db1 ac 0 0 abc def '101&102' "
        "2 2 1 101 102 1001 3\n"
        "ms_alloc_scratch db1 scratch1\n"
        "ms_open_stream db1 stream1\n"
        "ms_scan_stream stream1 abc 3 scratch1 8\n"
        "ms_remove_database_rule db1 101 3 abc 1\n"
        "ms_scan_stream stream1 def 3 scratch1 8\n"
        "ms_reset_stream stream1\n"
        "ms_scan_stream stream1 def 3 scratch1 8\n"
        "ms_close_stream stream1 scratch1 8\n"
        "ms_free_scratch scratch1\n"
        "ms_free_database db1\n"
        "exit\n"
    },
    {
        "owner-error",
        "Expected scratch/database owner mismatch",
        "# Expected failure: OWNER_MISMATCH and non-zero session exit\n"
        "ms_compile_database_multi db1 ac 0 0 abc 0 101 1\n"
        "ms_compile_database_multi db2 ac 0 0 def 0 102 1\n"
        "ms_alloc_scratch db1 scratch1\n"
        "ms_scan_database db2 def 3 scratch1 8\n"
        "ms_free_scratch scratch1\n"
        "ms_free_database db1\n"
        "ms_free_database db2\n"
        "exit\n"
    }
};

void demo_list_flows(void)
{
    size_t i;
    printf("FLOW\tDESCRIPTION\n");
    for (i = 0; i < sizeof(g_flows) / sizeof(g_flows[0]); i++) {
        printf("%s\t%s\n", g_flows[i].name, g_flows[i].summary);
    }
}

int demo_print_flow(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof(g_flows) / sizeof(g_flows[0]); i++) {
        if (strcmp(name, g_flows[i].name) == 0) {
            printf("# flow=%s\n# %s\n", g_flows[i].name,
                   g_flows[i].summary);
            printf("%s", g_flows[i].script);
            return 0;
        }
    }
    printf("unknown flow: %s\n", name);
    demo_list_flows();
    return 1;
}

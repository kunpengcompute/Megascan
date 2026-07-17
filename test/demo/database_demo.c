/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "demo_common.h"

#include <stdio.h>
#include <string.h>

#include "ms.h"
#include "ms_runtime.h"

static const char *const g_content_gateway_apis[] = {
    "ms_create_database",
    "ms_compile_database_multi_ext",
    "ms_free_database",
    "ms_alloc_scratch",
    "ms_free_scratch",
    "ms_scan_database",
    "ms_find_database_rule",
    NULL
};

static const char *const g_stream_audit_apis[] = {
    "ms_compile_database_multi",
    "ms_free_database",
    "ms_alloc_scratch",
    "ms_free_scratch",
    "ms_open_stream",
    "ms_scan_stream",
    "ms_flush_stream",
    "ms_reset_stream",
    "ms_close_stream",
    NULL
};

static const char *const g_ioc_lookup_apis[] = {
    "ms_compile_database_multi",
    "ms_free_database",
    "ms_find_database_rule",
    NULL
};

const demo_scenario_t g_demo_scenario_content_gateway = {
    "content-gateway",
    "内容网关扫描",
    "编译 AC database，并以块模式扫描请求内容。",
    g_content_gateway_apis,
    demo_database_block_apis
};

const demo_scenario_t g_demo_scenario_stream_audit = {
    "stream-audit",
    "流式审计",
    "打开 database stream，扫描拆分为多个数据块的消息。",
    g_stream_audit_apis,
    demo_database_stream_apis
};

const demo_scenario_t g_demo_scenario_ioc_lookup = {
    "ioc-lookup",
    "IOC 精确查询",
    "编译 HASH database，并通过 key 精确查询 IOC。",
    g_ioc_lookup_apis,
    demo_database_hash_apis
};

void demo_database_block_apis(void)
{
    const demo_options_t *options = demo_get_options();
    ms_database_t *empty_db = NULL;
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    const uint8_t *payload = options->input != NULL ? options->input :
                             (const uint8_t *)"POST /pay password=secret-value&token=session-token";
    uint32_t payload_len = options->input != NULL ? options->input_len :
                           (uint32_t)strlen((const char *)payload);
    const char *expr[] = {
        options->pattern != NULL ? options->pattern : "password=", "token=", "101&102"
    };
    uint32_t ids[] = {101, 102, 2001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    ms_expr_ext_t ext[] = {
        {0, 0, 0},
        {0, 0, 0},
        {MS_EXT_FLAG_MAX_OFFSET, 0, 64}
    };
    uint32_t out[4] = {0};
    uint32_t count = 0;
    ms_rule_t query = {0, payload_len, (uint8_t *)payload};
    int ret;

    demo_section("内容网关扫描");

    ret = demo_step_ret("创建空 AC database",
                        ms_create_database(&empty_db, MS_MATCHER_TYPE_AC, 0, 0));
    if (ret == MS_SUCCESS) {
        (void)demo_step_ret("释放空 AC database", ms_free_database(empty_db));
    }

    ret = demo_step_ret("编译 AC database",
                        ms_compile_database_multi_ext(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                                      expr, flags, ids, ext, 3));
    if (ret != MS_SUCCESS) {
        return;
    }
    ret = demo_step_ret("分配 scratch", ms_alloc_scratch(db, &scratch));
    if (ret != MS_SUCCESS) {
        (void)ms_free_database(db);
        return;
    }
    (void)demo_step_ret("扫描块数据",
                        ms_scan_database(db, payload, payload_len,
                                         scratch, out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
    demo_print_ids("ms_scan_database 输出", out, count);
    (void)demo_step_ret("通过 database 查询 AC 规则", ms_find_database_rule(db, &query));
    demo_note("ms_find_database_rule(AC) 输出", "返回 MS_SUCCESS 表示命中");
    (void)demo_step_ret("扫描未命中数据",
                        ms_scan_database(db, (const uint8_t *)"GET /health", 11,
                                         scratch, out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
    (void)demo_step_ret("释放 scratch", ms_free_scratch(scratch));
    (void)demo_step_ret("释放 AC database", ms_free_database(db));
}

void demo_database_stream_apis(void)
{
    const demo_options_t *options = demo_get_options();
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {
        options->pattern != NULL ? options->pattern : "session=", "blocked", "101&102"
    };
    uint32_t ids[] = {101, 102, 3001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out[4] = {0};
    uint32_t count = 0;
    int ret;

    demo_section("流式审计");

    ret = demo_step_ret("编译流式 AC database",
                        ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0, expr, flags, ids, 3));
    if (ret != MS_SUCCESS) {
        return;
    }
    ret = demo_step_ret("分配流式扫描 scratch", ms_alloc_scratch(db, &scratch));
    if (ret != MS_SUCCESS) {
        (void)ms_free_database(db);
        return;
    }
    (void)demo_step_ret("打开 database stream", ms_open_stream(db, &stream));
    if (stream != NULL) {
        if (options->input != NULL) {
            uint32_t offset = 0;
            uint32_t chunk_size = options->chunk_size != 0 ? options->chunk_size : options->input_len;

            while (offset < options->input_len) {
                uint32_t remaining = options->input_len - offset;
                uint32_t length = remaining < chunk_size ? remaining : chunk_size;

                count = 0;
                (void)demo_step_ret("扫描用户流数据块",
                                    ms_scan_stream(stream, options->input + offset, length, scratch,
                                                   out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
                demo_print_ids("ms_scan_stream 输出", out, count);
                offset += length;
            }
        } else {
            (void)demo_step_ret("扫描第 1 个流数据块",
                                ms_scan_stream(stream, (const uint8_t *)"event session=", 14, scratch,
                                               out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
            demo_print_ids("ms_scan_stream 第 1 块输出", out, count);

            count = 0;
            (void)demo_step_ret("扫描第 2 个流数据块",
                                ms_scan_stream(stream, (const uint8_t *)"uid42 blocked", 13, scratch,
                                               out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
            demo_print_ids("ms_scan_stream 第 2 块输出", out, count);
        }

        count = 0;
        (void)demo_step_ret("刷新 database stream",
                            ms_flush_stream(stream, scratch, out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
        (void)demo_step_ret("重置 database stream", ms_reset_stream(stream));
        count = 0;
        (void)demo_step_ret("关闭 database stream",
                            ms_close_stream(stream, scratch, out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
    }
    (void)demo_step_ret("释放流式扫描 scratch", ms_free_scratch(scratch));
    (void)demo_step_ret("释放流式 database", ms_free_database(db));
}

void demo_database_hash_apis(void)
{
    const demo_options_t *options = demo_get_options();
    const char *lookup_key = options->key != NULL ? options->key : "malware-sha256-demo";
    ms_database_t *db = NULL;
    const char *expr[] = {"bad-domain.example", lookup_key};
    uint32_t ids[] = {4001, 4002};
    ms_rule_t query = demo_make_rule(0, lookup_key);
    int ret;

    demo_section("IOC 精确查询");

    ret = demo_step_ret("编译 HASH database",
                        ms_compile_database_multi(&db, MS_MATCHER_TYPE_HASH, 16, 4, expr, NULL, ids, 2));
    if (ret != MS_SUCCESS) {
        return;
    }
    (void)demo_step_ret("通过 database 查询 HASH key", ms_find_database_rule(db, &query));
    printf("%-48s 命中alias=%u\n", "ms_find_database_rule(HASH) 输出", query.alias);
    (void)demo_step_ret("释放 HASH database", ms_free_database(db));
}

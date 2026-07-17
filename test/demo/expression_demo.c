/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "demo_common.h"

#include "ms_compile.h"
#include "ms_expr.h"
#include "ms_handler.h"
#include "ms_runtime.h"
#include <string.h>

static const char *const g_security_policy_apis[] = {
    "ms_handler_init",
    "ms_handler_destroy",
    "ms_compile_multi",
    "ms_compile_multi_ext",
    "ms_add_exprs",
    "ms_scan_expr",
    "ms_compile_eval_combinations",
    "ms_compile_scan_ac_combinations",
    "ms_open_handler_stream",
    "ms_scan_expr_stream",
    "ms_scan_rule_stream",
    "ms_scan_expr_stream_flush",
    "ms_reset_stream",
    "ms_close_handler_stream",
    NULL
};

const demo_scenario_t g_demo_scenario_security_policy = {
    "security-policy",
    "安全策略匹配",
    "编译普通、quiet 和组合规则，识别包含多个信号的安全事件。",
    g_security_policy_apis,
    demo_legacy_expression_apis
};

void demo_legacy_expression_apis(void)
{
    const demo_options_t *options = demo_get_options();
    const char *primary_pattern = options->pattern != NULL ? options->pattern : "login";
    const uint8_t *input = options->input != NULL ? options->input :
                           (const uint8_t *)"user=admin login failed";
    uint32_t input_len = options->input != NULL ? options->input_len :
                         (uint32_t)strlen((const char *)input);
    ms_handler_t handler = {0};
    ms_handler_t compile_multi_handler = {0};
    ms_handler_t add_exprs_handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {primary_pattern, "failed", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    ms_expr_ext_t ext[] = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    };
    const char *add_exprs[] = {"api-key=", "secret="};
    uint32_t add_ids[] = {1201, 1202};
    uint32_t add_flags[] = {MS_FLAG_NONE, MS_FLAG_NONE};
    const char *simple_expr[] = {"download", "blocked"};
    uint32_t simple_ids[] = {1301, 1302};
    uint32_t simple_flags[] = {MS_FLAG_NONE, MS_FLAG_NONE};
    uint32_t out[4] = {0};
    uint32_t count = 0;
    uint32_t aliases[] = {101, 102};
    uint32_t comb_ids[2] = {0};
    uint32_t comb_count = 0;
    int ret;

    demo_section("安全策略匹配");

    ret = demo_step_ret("创建 AC handler", ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0));
    if (ret != MS_SUCCESS) {
        return;
    }
    ret = demo_step_ret("编译带扩展参数的表达式",
                        ms_compile_multi_ext(&handler, expr, flags, ids, ext, 3));
    if (ret != MS_SUCCESS) {
        (void)ms_handler_destroy(&handler);
        return;
    }
    (void)demo_step_ret("扫描块数据",
                        ms_scan_expr(&handler, input, input_len,
                                     out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
    demo_print_ids("ms_scan_expr 输出", out, count);

    (void)demo_step_ret("根据 alias 计算组合规则",
                        ms_compile_eval_combinations(&handler, aliases, 2, comb_ids, &comb_count, 2));
    demo_print_ids("ms_compile_eval_combinations 输出", comb_ids, comb_count);

    comb_count = 0;
    (void)demo_step_ret("扫描并返回组合规则 ID",
                        ms_compile_scan_ac_combinations(&handler, input, input_len,
                                                        comb_ids, &comb_count, 2));
    demo_print_ids("ms_compile_scan_ac_combinations 输出", comb_ids, comb_count);

    (void)demo_step_ret("打开 handler stream", ms_open_handler_stream(&handler, &stream));
    if (stream != NULL) {
        count = 0;
        (void)demo_step_ret("扫描第 1 个流数据块",
                            ms_scan_expr_stream(stream, (const uint8_t *)"user login", 10,
                                                out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
        demo_print_ids("ms_scan_expr_stream 第 1 块输出", out, count);

        count = 0;
        (void)demo_step_ret("扫描第 2 个流数据块", ms_scan_rule_stream(stream, (const uint8_t *)" failed", 7));
        (void)demo_step_ret("刷新 handler stream",
                            ms_scan_expr_stream_flush(stream, out, &count, (uint32_t)DEMO_ARRAY_SIZE(out)));
        (void)demo_step_ret("重置 handler stream", ms_reset_stream(stream));
        (void)demo_step_ret("关闭 handler stream", ms_close_handler_stream(stream));
    }
    (void)demo_step_ret("销毁表达式 handler", ms_handler_destroy(&handler));

    ret = demo_step_ret("为 ms_compile_multi 创建 handler",
                        ms_handler_init(&compile_multi_handler, MS_MATCHER_TYPE_AC, 0, 0));
    if (ret == MS_SUCCESS) {
        (void)demo_step_ret("编译普通表达式",
                            ms_compile_multi(&compile_multi_handler,
                                             simple_expr,
                                             simple_flags,
                                             simple_ids,
                                             2));
        (void)demo_step_ret("销毁 ms_compile_multi handler",
                            ms_handler_destroy(&compile_multi_handler));
    }

    ret = demo_step_ret("为 ms_add_exprs 创建 handler",
                        ms_handler_init(&add_exprs_handler, MS_MATCHER_TYPE_AC, 0, 0));
    if (ret != MS_SUCCESS) {
        return;
    }
    (void)demo_step_ret("通过兼容接口添加表达式",
                        ms_add_exprs(&add_exprs_handler, add_exprs, add_ids, add_flags, 2));
    (void)demo_step_ret("销毁 ms_add_exprs handler", ms_handler_destroy(&add_exprs_handler));
}

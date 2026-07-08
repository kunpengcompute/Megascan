/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: expression-style stream compatibility API unit tests
 */
#include <stdint.h>
#include <string.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"
#include "../../../ms/include/ms_handler.h"
#include "../../../ms/include/ms_compile.h"
#include "../../../ms/include/ms_rule.h"
#include "../../../ms/include/ms_runtime.h"
#include "../../../ms/include/ms_expr.h"

static ms_rule_t make_expr_stream_query(const char *s)
{
    ms_rule_t rule;

    rule.alias = 0;
    rule.key_len = (uint32_t)strlen(s);
    rule.key = (uint8_t *)s;

    return rule;
}

static void test_expr_stream_accumulates_combination(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"xxabc", 5,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"yydef", 5,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_reports_plain_and_then_combination(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[2] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"xxabc", 5,
                                    out_ids, &out_count, 2), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"yydef", 5,
                                    out_ids, &out_count, 2), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_deduplicates_combination_reports(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"zzzdef", 6,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_reset_clears_logical_state(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_capacity_retry_preserves_state(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[2] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"xxabcdefyy", 10,
                                    out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"xxabcdefyy", 10,
                                    out_ids, &out_count, 2), MS_SUCCESS);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(out_ids[1], 1001U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_stale_reset_rebuilds_expr_state(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;
    ms_rule_t remove_rule = make_expr_stream_query("abc");

    remove_rule.alias = 101;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_remove_rule(&handler, &remove_rule, 1), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MS_STREAM_STALE);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_active_ckey_skips_unrelated_combinations(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "foo", "bar", "101&102", "201&202"};
    uint32_t ids[] = {101, 102, 201, 202, 1001, 1002};
    uint32_t flags[] = {
        MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_QUIET,
        MS_FLAG_QUIET, MS_FLAG_COMBINATION, MS_FLAG_COMBINATION
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 6), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"xxabc", 5,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"yydef", 5,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_inplace_logical_ops_nested(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"a", "b", "c", "(101|102)&!103"};
    uint32_t ids[] = {101, 102, 103, 1001};
    uint32_t flags[] = {
        MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_QUIET,
        MS_FLAG_COMBINATION
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 4), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"xxa", 3,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"xxa", 3,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"c", 1,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_flush_reports_pure_negative_once(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "!101"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"xxx", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_expr_stream_flush(stream, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_scan_expr_stream_flush(stream, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_expr_stream_flush(stream, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_offset_bounds_use_cumulative_offset(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    ms_expr_ext_t ext[] = {
        {0, 0, 0},
        {0, 0, 0},
        {MS_EXT_FLAG_MIN_OFFSET | MS_EXT_FLAG_MAX_OFFSET, 6, 10}
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi_ext(&handler, expr, flags, ids, ext, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abcdefxxxxx", 11,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abcxxxxxdef", 11,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_combination_singlematch_flag(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {
        MS_FLAG_QUIET, MS_FLAG_QUIET,
        MS_FLAG_COMBINATION | MS_FLAG_SINGLEMATCH
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abcdef", 6,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abcdef", 6,
                                    out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_rule_scan_uses_expression_runtime(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_rule_stream(stream, (const uint8_t *)"abc", 3), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_rule_stream(stream, (const uint8_t *)"def", 3), MS_SUCCESS);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_stream_remove_combination_marks_stream_stale(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    ms_rule_t remove_comb = {0};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    remove_comb.alias = 1001;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_remove_rule(&handler, &remove_comb, 1), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MS_STREAM_STALE);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr_stream(stream, (const uint8_t *)"abcdef", 6,
                                    out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_accumulates_combination);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_reports_plain_and_then_combination);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_deduplicates_combination_reports);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_reset_clears_logical_state);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_capacity_retry_preserves_state);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_stale_reset_rebuilds_expr_state);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_active_ckey_skips_unrelated_combinations);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_inplace_logical_ops_nested);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_flush_reports_pure_negative_once);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_offset_bounds_use_cumulative_offset);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_combination_singlematch_flag);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_rule_scan_uses_expression_runtime);
KTEST_ADD_TEST_CASE(SuiteExprStreamCompat, test_expr_stream_remove_combination_marks_stream_stale);

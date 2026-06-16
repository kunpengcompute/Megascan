/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: expression-style stream API unit tests
 */
#include <stdint.h>
#include <string.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"
#include "../../../mmpm/include/mmpm_expr.h"

static mmpm_rule_t make_expr_stream_query(const char *s)
{
    mmpm_rule_t rule;

    rule.alias = 0;
    rule.key_len = (uint32_t)strlen(s);
    rule.key = (uint8_t *)s;

    return rule;
}

static void test_expr_stream_accumulates_combination(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"xxabc", 5,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"yydef", 5,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_reports_plain_and_then_combination(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[2] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"xxabc", 5,
                                    out_ids, &out_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"yydef", 5,
                                    out_ids, &out_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_deduplicates_combination_reports(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"zzzdef", 6,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_reset_clears_logical_state(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_capacity_retry_preserves_state(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[2] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"xxabcdefyy", 10,
                                    out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"xxabcdefyy", 10,
                                    out_ids, &out_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(out_ids[1], 1001U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_stale_reset_rebuilds_expr_state(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;
    mmpm_rule_t remove_rule = make_expr_stream_query("abc");

    remove_rule.alias = 101;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_remove_rule(&handler, &remove_rule, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MMPM_STREAM_STALE);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_active_ckey_skips_unrelated_combinations(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "foo", "bar", "101&102", "201&202"};
    uint32_t ids[] = {101, 102, 201, 202, 1001, 1002};
    uint32_t flags[] = {
        MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_QUIET,
        MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION, MMPM_FLAG_COMBINATION
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 6), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"xxabc", 5,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"yydef", 5,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_inplace_logical_ops_nested(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"a", "b", "c", "(101|102)&!103"};
    uint32_t ids[] = {101, 102, 103, 1001};
    uint32_t flags[] = {
        MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_QUIET,
        MMPM_FLAG_COMBINATION
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 4), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"xxa", 3,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"xxa", 3,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"c", 1,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_flush_reports_pure_negative_once(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "!101"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"xxx", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_expr_stream_flush(stream, out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_scan_expr_stream_flush(stream, out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_expr_stream_flush(stream, out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_offset_bounds_use_cumulative_offset(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    mmpm_expr_ext_t ext[] = {
        {0, 0, 0},
        {0, 0, 0},
        {MMPM_EXT_FLAG_MIN_OFFSET | MMPM_EXT_FLAG_MAX_OFFSET, 6, 10}
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi_ext(&handler, expr, flags, ids, ext, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abcdefxxxxx", 11,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abcxxxxxdef", 11,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_combination_singlematch_flag(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {
        MMPM_FLAG_QUIET, MMPM_FLAG_QUIET,
        MMPM_FLAG_COMBINATION | MMPM_FLAG_SINGLEMATCH
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abcdef", 6,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abcdef", 6,
                                    out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_rule_scan_uses_expression_runtime(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"def", 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_stream_remove_combination_marks_stream_stale(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    mmpm_rule_t remove_comb = {0};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    remove_comb.alias = 1001;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abc", 3,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_remove_rule(&handler, &remove_comb, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"def", 3,
                                    out_ids, &out_count, 1), MMPM_STREAM_STALE);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr_stream(stream, (const uint8_t *)"abcdef", 6,
                                    out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_accumulates_combination);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_reports_plain_and_then_combination);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_deduplicates_combination_reports);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_reset_clears_logical_state);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_capacity_retry_preserves_state);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_stale_reset_rebuilds_expr_state);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_active_ckey_skips_unrelated_combinations);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_inplace_logical_ops_nested);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_flush_reports_pure_negative_once);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_offset_bounds_use_cumulative_offset);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_combination_singlematch_flag);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_rule_scan_uses_expression_runtime);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_remove_combination_marks_stream_stale);

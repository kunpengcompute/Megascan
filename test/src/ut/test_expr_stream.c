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

KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_accumulates_combination);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_reports_plain_and_then_combination);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_deduplicates_combination_reports);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_reset_clears_logical_state);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_capacity_retry_preserves_state);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_stale_reset_rebuilds_expr_state);
KTEST_ADD_TEST_CASE(SuiteExprStream, test_expr_stream_active_ckey_skips_unrelated_combinations);

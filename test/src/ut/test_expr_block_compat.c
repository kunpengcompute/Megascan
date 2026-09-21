/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: expression-style block compatibility API unit tests
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

static ms_rule_t make_expr_block_query(const char *s)
{
    ms_rule_t rule;

    rule.alias = 0;
    rule.key_len = (uint32_t)strlen(s);
    rule.key = (uint8_t *)s;

    return rule;
}

static void test_expr_block_eval_combination_and_true(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t aliases[] = {101, 102};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_eval_combinations(&handler, aliases, 2, comb_ids, &comb_count, 1), MS_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_eval_combination_and_false(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t aliases[] = {101};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_eval_combinations(&handler, aliases, 1, comb_ids, &comb_count, 1), MS_SUCCESS);
    EXPECT_EQ(comb_count, 0U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_eval_combination_not_true(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&!102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t aliases[] = {101};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_eval_combinations(&handler, aliases, 1, comb_ids, &comb_count, 1), MS_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_eval_combination_large_ids(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "900000001&900000002"};
    uint32_t ids[] = {900000001, 900000002, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t aliases[] = {900000001, 900000002};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_eval_combinations(&handler, aliases, 2, comb_ids, &comb_count, 1), MS_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_eval_combination_capacity_too_small(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t aliases[] = {101, 102};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_eval_combinations(&handler, aliases, 2, NULL, &comb_count, 0), MS_INVALID_PARA);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_ac_combination_true(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcdefyy", 10,
                                                comb_ids, &comb_count, 1), MS_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_ac_combination_false(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcyy", 7,
                                                comb_ids, &comb_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(comb_count, 0U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_ac_combination_not_true(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&!102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcyy", 7,
                                                comb_ids, &comb_count, 1), MS_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_ac_combination_capacity_too_small(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcdefyy", 10,
                                                NULL, &comb_count, 0), MS_INVALID_PARA);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_reports_plain_and_combination(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[2] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxabcdefyy", 10,
                             out_ids, &out_count, 2), MS_SUCCESS);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(out_ids[1], 1001U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_quiet_only_no_report(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MS_FLAG_QUIET};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxabcyy", 7,
                             out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_capacity_too_small(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxabcdefyy", 10,
                             out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(out_ids[0], 0U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_deduplicates_plain_matches(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, NULL, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abcabc", 6,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_active_ckey_skips_unrelated_combinations(void)
{
    ms_handler_t handler = {0};
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
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxabcdefyy", 10,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_active_ckey_shared_lkey(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "ghi", "101&102", "101&103"};
    uint32_t ids[] = {101, 102, 103, 1001, 1002};
    uint32_t flags[] = {
        MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_QUIET,
        MS_FLAG_COMBINATION, MS_FLAG_COMBINATION
    };
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 5), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxabcghiyy", 10,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1002U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_inplace_logical_ops_nested(void)
{
    ms_handler_t handler = {0};
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
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxayy", 5,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxacyy", 6,
                             out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_final_flush_pure_negative(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "!101"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxxx", 4,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abc", 3,
                             out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_scan_offset_bounds(void)
{
    ms_handler_t handler = {0};
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
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abcdef", 6,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abcdefxxxxx", 11,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abcxxxxxdef", 11,
                             out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_remove_combination_by_id(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    ms_rule_t remove_comb = {0};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    remove_comb.alias = 1001;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abcdef", 6,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_remove_rule(&handler, &remove_comb, 1), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abcdef", 6,
                             out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_compile_multi(&handler, &expr[2], &flags[2], &ids[2], 1), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_remove_plain_removes_dependent_combination(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    ms_rule_t remove_plain = make_expr_block_query("abc");
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    remove_plain.alias = 101;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abcdef", 6,
                             out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(ms_remove_rule(&handler, &remove_plain, 1), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"abcdef", 6,
                             out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_block_remove_rebuilds_final_ckeys(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "!101", "!102"};
    uint32_t ids[] = {101, 102, 1001, 1002};
    uint32_t flags[] = {
        MS_FLAG_QUIET, MS_FLAG_QUIET,
        MS_FLAG_COMBINATION, MS_FLAG_COMBINATION
    };
    ms_rule_t remove_comb = {0};
    uint32_t out_ids[2] = {0};
    uint32_t out_count = 0;

    remove_comb.alias = 1001;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 4), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxxx", 4,
                             out_ids, &out_count, 2), MS_SUCCESS);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(out_ids[1], 1002U);

    EXPECT_EQ(ms_remove_rule(&handler, &remove_comb, 1), MS_SUCCESS);
    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxxx", 4,
                             out_ids, &out_count, 2), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1002U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_eval_combination_and_true);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_eval_combination_and_false);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_eval_combination_not_true);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_eval_combination_large_ids);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_eval_combination_capacity_too_small);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_ac_combination_true);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_ac_combination_false);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_ac_combination_not_true);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_ac_combination_capacity_too_small);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_reports_plain_and_combination);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_quiet_only_no_report);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_capacity_too_small);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_deduplicates_plain_matches);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_active_ckey_skips_unrelated_combinations);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_active_ckey_shared_lkey);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_inplace_logical_ops_nested);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_final_flush_pure_negative);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_scan_offset_bounds);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_remove_combination_by_id);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_remove_plain_removes_dependent_combination);
KTEST_ADD_TEST_CASE(SuiteExprBlockCompat, test_expr_block_remove_rebuilds_final_ckeys);

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: expression-style API unit tests
 */
#include <stdint.h>
#include <string.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"
#include "../../../mmpm/include/mmpm_expr.h"

static mmpm_rule_t make_expr_query(const char *s)
{
    mmpm_rule_t rule;

    rule.alias = 0;
    rule.key_len = (uint32_t)strlen(s);
    rule.key = (uint8_t *)s;

    return rule;
}

static void test_expr_api_hash_adds_plain_exprs(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def"};
    uint32_t ids[] = {101, 102};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET};
    mmpm_rule_t query = make_expr_query("def");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(query.alias, 102U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_ac_adds_plain_exprs(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"123", "abcdef"};
    uint32_t ids[] = {201, 202};
    mmpm_rule_t query = make_expr_query("xxabcdefyy");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, NULL, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_compiles_combination(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    mmpm_rule_t query = make_expr_query("def");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(query.alias, 102U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_rejects_unknown_combination_id(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "101&999"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_COMBINATION};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 2), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_rejects_combination_of_combination(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "101", "1001&101"};
    uint32_t ids[] = {101, 1001, 1002};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_COMBINATION, MMPM_FLAG_COMBINATION};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_rejects_duplicate_ids(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def"};
    uint32_t ids[] = {101, 101};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, NULL, ids, 2), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_rejects_combination_quiet(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "101"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_COMBINATION | MMPM_FLAG_QUIET};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 2), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_combination_can_reference_previous_compile(void)
{
    mmpm_handler_t handler = {0};
    const char *plain_expr[] = {"abc", "def"};
    uint32_t plain_ids[] = {900000001, 900000002};
    const char *comb_expr[] = {"900000001&900000002"};
    uint32_t comb_ids[] = {1001};
    uint32_t comb_flags[] = {MMPM_FLAG_COMBINATION};
    mmpm_rule_t query = make_expr_query("def");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, plain_expr, NULL, plain_ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, comb_expr, comb_flags, comb_ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(query.alias, 900000002U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_eval_combination_and_true(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t aliases[] = {101, 102};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_eval_combinations(&handler, aliases, 2, comb_ids, &comb_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_eval_combination_and_false(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t aliases[] = {101};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_eval_combinations(&handler, aliases, 1, comb_ids, &comb_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(comb_count, 0U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_eval_combination_not_true(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&!102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t aliases[] = {101};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_eval_combinations(&handler, aliases, 1, comb_ids, &comb_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_eval_combination_large_ids(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "900000001&900000002"};
    uint32_t ids[] = {900000001, 900000002, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t aliases[] = {900000001, 900000002};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_eval_combinations(&handler, aliases, 2, comb_ids, &comb_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_eval_combination_capacity_too_small(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t aliases[] = {101, 102};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_eval_combinations(&handler, aliases, 2, NULL, &comb_count, 0), MMPM_INVALID_PARA);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_scan_ac_combination_true(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcdefyy", 10,
                                                comb_ids, &comb_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_scan_ac_combination_false(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcyy", 7,
                                                comb_ids, &comb_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(comb_count, 0U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_scan_ac_combination_not_true(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&!102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcyy", 7,
                                                comb_ids, &comb_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(comb_ids[0], 1001U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_scan_ac_combination_capacity_too_small(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcdefyy", 10,
                                                NULL, &comb_count, 0), MMPM_INVALID_PARA);
    EXPECT_EQ(comb_count, 1U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_scan_expr_reports_plain_and_combination(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[2] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr(&handler, (const uint8_t *)"xxabcdefyy", 10,
                             out_ids, &out_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(out_ids[1], 1001U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_scan_expr_quiet_only_no_report(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MMPM_FLAG_QUIET};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr(&handler, (const uint8_t *)"xxabcyy", 7,
                             out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_scan_expr_capacity_too_small(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr(&handler, (const uint8_t *)"xxabcdefyy", 10,
                             out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(out_count, 2U);
    EXPECT_EQ(out_ids[0], 0U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_api_scan_expr_deduplicates_plain_matches(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, NULL, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_expr(&handler, (const uint8_t *)"abcabc", 6,
                             out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_hash_adds_plain_exprs);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_ac_adds_plain_exprs);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_compiles_combination);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_rejects_unknown_combination_id);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_rejects_combination_of_combination);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_rejects_duplicate_ids);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_rejects_combination_quiet);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_combination_can_reference_previous_compile);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_eval_combination_and_true);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_eval_combination_and_false);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_eval_combination_not_true);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_eval_combination_large_ids);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_eval_combination_capacity_too_small);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_scan_ac_combination_true);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_scan_ac_combination_false);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_scan_ac_combination_not_true);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_scan_ac_combination_capacity_too_small);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_scan_expr_reports_plain_and_combination);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_scan_expr_quiet_only_no_report);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_scan_expr_capacity_too_small);
KTEST_ADD_TEST_CASE(SuiteExprApi, test_expr_api_scan_expr_deduplicates_plain_matches);

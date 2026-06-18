/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: expression-style compile and validation unit tests
 */
#include <stdint.h>
#include <string.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"
#include "../../../mmpm/include/mmpm_handler.h"
#include "../../../mmpm/include/mmpm_compile.h"
#include "../../../mmpm/include/mmpm_rule.h"
#include "../../../mmpm/include/mmpm_runtime.h"
#include "../../../mmpm/include/mmpm_expr.h"

static mmpm_rule_t make_expr_compile_query(const char *s)
{
    mmpm_rule_t rule;

    rule.alias = 0;
    rule.key_len = (uint32_t)strlen(s);
    rule.key = (uint8_t *)s;

    return rule;
}

static void test_expr_compile_hash_adds_plain_exprs(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def"};
    uint32_t ids[] = {101, 102};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET};
    mmpm_rule_t query = make_expr_compile_query("def");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(query.alias, 102U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_compile_ac_adds_plain_exprs(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"123", "abcdef"};
    uint32_t ids[] = {201, 202};
    mmpm_rule_t query = make_expr_compile_query("xxabcdefyy");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, NULL, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_compile_compiles_combination(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    mmpm_rule_t query = make_expr_compile_query("def");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(query.alias, 102U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_compile_rejects_unknown_combination_id(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "101&999"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_COMBINATION};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 2), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_compile_rejects_combination_of_combination(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "101", "1001&101"};
    uint32_t ids[] = {101, 1001, 1002};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_COMBINATION, MMPM_FLAG_COMBINATION};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_compile_rejects_duplicate_ids(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def"};
    uint32_t ids[] = {101, 101};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, NULL, ids, 2), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_compile_skips_quiet_combination(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "101"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_COMBINATION | MMPM_FLAG_QUIET};
    mmpm_rule_t query = make_expr_compile_query("abc");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(query.alias, 101U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_compile_combination_can_reference_previous_compile(void)
{
    mmpm_handler_t handler = {0};
    const char *plain_expr[] = {"abc", "def"};
    uint32_t plain_ids[] = {900000001, 900000002};
    const char *comb_expr[] = {"900000001&900000002"};
    uint32_t comb_ids[] = {1001};
    uint32_t comb_flags[] = {MMPM_FLAG_COMBINATION};
    mmpm_rule_t query = make_expr_compile_query("def");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, plain_expr, NULL, plain_ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, comb_expr, comb_flags, comb_ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_find_rule(&handler, &query), MMPM_SUCCESS);
    EXPECT_EQ(query.alias, 900000002U);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_expr_compile_rejects_plain_singlematch(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MMPM_FLAG_SINGLEMATCH};

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_hash_adds_plain_exprs);
KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_ac_adds_plain_exprs);
KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_compiles_combination);
KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_rejects_unknown_combination_id);
KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_rejects_combination_of_combination);
KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_rejects_duplicate_ids);
KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_skips_quiet_combination);
KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_combination_can_reference_previous_compile);
KTEST_ADD_TEST_CASE(SuiteExprCompile, test_expr_compile_rejects_plain_singlematch);

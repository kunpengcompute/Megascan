/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: expression-style compile and validation unit tests
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

static ms_rule_t make_expr_compile_query(const char *s)
{
    ms_rule_t rule;

    rule.alias = 0;
    rule.key_len = (uint32_t)strlen(s);
    rule.key = (uint8_t *)s;

    return rule;
}

static void test_expr_compile_hash_adds_plain_exprs(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def"};
    uint32_t ids[] = {101, 102};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET};
    ms_rule_t query = make_expr_compile_query("def");

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_find_rule(&handler, &query), MS_SUCCESS);
    EXPECT_EQ(query.alias, 102U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_compile_ac_adds_plain_exprs(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"123", "abcdef"};
    uint32_t ids[] = {201, 202};
    ms_rule_t query = make_expr_compile_query("xxabcdefyy");

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, NULL, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_find_rule(&handler, &query), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_compile_compiles_combination(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    ms_rule_t query = make_expr_compile_query("def");

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_find_rule(&handler, &query), MS_SUCCESS);
    EXPECT_EQ(query.alias, 102U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_compile_rejects_unknown_combination_id(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "101&999"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_COMBINATION};

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 2), MS_INVALID_PARA);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_compile_rejects_combination_of_combination(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "101", "1001&101"};
    uint32_t ids[] = {101, 1001, 1002};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_COMBINATION, MS_FLAG_COMBINATION};

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_INVALID_PARA);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_compile_rejects_duplicate_ids(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def"};
    uint32_t ids[] = {101, 101};

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, NULL, ids, 2), MS_INVALID_PARA);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_compile_skips_quiet_combination(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "101"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_COMBINATION | MS_FLAG_QUIET};
    ms_rule_t query = make_expr_compile_query("abc");

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_find_rule(&handler, &query), MS_SUCCESS);
    EXPECT_EQ(query.alias, 101U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_compile_combination_can_reference_previous_compile(void)
{
    ms_handler_t handler = {0};
    const char *plain_expr[] = {"abc", "def"};
    uint32_t plain_ids[] = {900000001, 900000002};
    const char *comb_expr[] = {"900000001&900000002"};
    uint32_t comb_ids[] = {1001};
    uint32_t comb_flags[] = {MS_FLAG_COMBINATION};
    ms_rule_t query = make_expr_compile_query("def");

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, plain_expr, NULL, plain_ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, comb_expr, comb_flags, comb_ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_find_rule(&handler, &query), MS_SUCCESS);
    EXPECT_EQ(query.alias, 900000002U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_expr_compile_rejects_plain_singlematch(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MS_FLAG_SINGLEMATCH};

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
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

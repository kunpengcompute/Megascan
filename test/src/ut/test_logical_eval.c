/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: logical vector evaluation unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"
#include "../../../mmpm/include/mmpm_logical.h"

#define LOGICAL_COUNT 128U
#define LOGICAL_BYTES ((LOGICAL_COUNT + 7U) / 8U)

static void test_logical_vector_set_get_clear(void)
{
    uint8_t logical_vector[LOGICAL_BYTES] = {0};
    uint8_t value = 0;

    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 101, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_get(logical_vector, LOGICAL_COUNT, 101, &value), MMPM_SUCCESS);
    EXPECT_EQ(value, 1U);

    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 101, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_get(logical_vector, LOGICAL_COUNT, 101, &value), MMPM_SUCCESS);
    EXPECT_EQ(value, 0U);

    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 127, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_clear(logical_vector, LOGICAL_COUNT), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_get(logical_vector, LOGICAL_COUNT, 127, &value), MMPM_SUCCESS);
    EXPECT_EQ(value, 0U);
}

static void test_logical_eval_and_true(void)
{
    mmpm_logical_tree_t tree;
    uint8_t logical_vector[LOGICAL_BYTES] = {0};
    uint8_t matched = 0;

    EXPECT_EQ(mmpm_logical_parse("101&102", &tree), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 101, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 102, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_eval(&tree, logical_vector, LOGICAL_COUNT, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 1U);
    mmpm_logical_tree_destroy(&tree);
}

static void test_logical_eval_not_false(void)
{
    mmpm_logical_tree_t tree;
    uint8_t logical_vector[LOGICAL_BYTES] = {0};
    uint8_t matched = 0;

    EXPECT_EQ(mmpm_logical_parse("101&!102", &tree), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 101, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 102, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_eval(&tree, logical_vector, LOGICAL_COUNT, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 0U);
    mmpm_logical_tree_destroy(&tree);
}

static void test_logical_eval_precedence_true(void)
{
    mmpm_logical_tree_t tree;
    uint8_t logical_vector[LOGICAL_BYTES] = {0};
    uint8_t matched = 0;

    EXPECT_EQ(mmpm_logical_parse("(101|102)&!103", &tree), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 101, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 102, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, 103, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_eval(&tree, logical_vector, LOGICAL_COUNT, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 1U);
    mmpm_logical_tree_destroy(&tree);
}

static int eval_logical_expr(const char *expr, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t *matched)
{
    mmpm_logical_tree_t tree;
    uint8_t logical_vector[LOGICAL_BYTES] = {0};
    int ret;

    ret = mmpm_logical_parse(expr, &tree);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = mmpm_logical_set(logical_vector, LOGICAL_COUNT, 1, v1);
    if (ret == MMPM_SUCCESS) {
        ret = mmpm_logical_set(logical_vector, LOGICAL_COUNT, 2, v2);
    }
    if (ret == MMPM_SUCCESS) {
        ret = mmpm_logical_set(logical_vector, LOGICAL_COUNT, 3, v3);
    }
    if (ret == MMPM_SUCCESS) {
        ret = mmpm_logical_eval(&tree, logical_vector, LOGICAL_COUNT, matched);
    }

    mmpm_logical_tree_destroy(&tree);
    return ret;
}

static void test_logical_eval_ultrascan_operator_precedence(void)
{
    uint8_t matched = 0;

    /* Ultrascan-style precedence: ! > & > |, so !1|2&3 means (!1)|(2&3). */
    EXPECT_EQ(eval_logical_expr("!1|2&3", 1, 1, 1, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 1U);

    EXPECT_EQ(eval_logical_expr("!1|2&3", 1, 1, 0, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 0U);

    EXPECT_EQ(eval_logical_expr("!1|2&3", 0, 0, 0, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 1U);
}

static void test_logical_eval_ultrascan_parentheses_override_precedence(void)
{
    uint8_t matched = 0;

    /* Parentheses force (!1|2) to be evaluated before &3. */
    EXPECT_EQ(eval_logical_expr("(!1|2)&3", 0, 0, 0, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 0U);

    EXPECT_EQ(eval_logical_expr("(!1|2)&3", 0, 0, 1, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 1U);
}

static void test_logical_eval_ultrascan_and_before_or(void)
{
    uint8_t matched = 0;

    /* 1|2&3 means 1|(2&3), not (1|2)&3. */
    EXPECT_EQ(eval_logical_expr("1|2&3", 1, 0, 0, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 1U);

    EXPECT_EQ(eval_logical_expr("1|2&3", 0, 1, 0, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 0U);
}

static void test_logical_eval_ultrascan_left_associative_same_precedence(void)
{
    uint8_t matched = 0;

    /* Same-precedence operators are left associative: 1&2&3 means (1&2)&3. */
    EXPECT_EQ(eval_logical_expr("1&2&3", 1, 1, 1, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 1U);

    EXPECT_EQ(eval_logical_expr("1&2&3", 1, 1, 0, &matched), MMPM_SUCCESS);
    EXPECT_EQ(matched, 0U);
}

static void test_logical_eval_invalid_args(void)
{
    mmpm_logical_tree_t tree;
    uint8_t logical_vector[LOGICAL_BYTES] = {0};
    uint8_t matched = 0;
    uint8_t value = 0;

    EXPECT_EQ(mmpm_logical_parse("101&102", &tree), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_logical_set(NULL, LOGICAL_COUNT, 101, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_logical_get(NULL, LOGICAL_COUNT, 101, &value), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_logical_get(logical_vector, LOGICAL_COUNT, 101, NULL), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_logical_set(logical_vector, LOGICAL_COUNT, LOGICAL_COUNT, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_logical_eval(NULL, logical_vector, LOGICAL_COUNT, &matched), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_logical_eval(&tree, NULL, LOGICAL_COUNT, &matched), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_logical_eval(&tree, logical_vector, LOGICAL_COUNT, NULL), MMPM_INVALID_PARA);
    mmpm_logical_tree_destroy(&tree);
}

KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_vector_set_get_clear);
KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_eval_and_true);
KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_eval_not_false);
KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_eval_precedence_true);
KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_eval_ultrascan_operator_precedence);
KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_eval_ultrascan_parentheses_override_precedence);
KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_eval_ultrascan_and_before_or);
KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_eval_ultrascan_left_associative_same_precedence);
KTEST_ADD_TEST_CASE(SuiteLogicalEval, test_logical_eval_invalid_args);

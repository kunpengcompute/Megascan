/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: logical expression parser unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"
#include "../../../ms/include/ms_logical.h"

static void test_logical_parse_and(void)
{
    ms_logical_tree_t tree;

    EXPECT_EQ(ms_logical_parse("101&102", &tree), MS_SUCCESS);
    EXPECT_EQ(tree.op_count, 1U);
    EXPECT_EQ(tree.ops[0].op, (uint32_t)MS_LOGICAL_OP_AND);
    EXPECT_EQ(tree.ops[0].left, 101U);
    EXPECT_EQ(tree.ops[0].right, 102U);
    EXPECT_EQ(tree.result, tree.ops[0].id);
    ms_logical_tree_destroy(&tree);
}

static void test_logical_parse_not(void)
{
    ms_logical_tree_t tree;

    EXPECT_EQ(ms_logical_parse("!101", &tree), MS_SUCCESS);
    EXPECT_EQ(tree.op_count, 1U);
    EXPECT_EQ(tree.ops[0].op, (uint32_t)MS_LOGICAL_OP_NOT);
    EXPECT_EQ(tree.ops[0].right, 101U);
    EXPECT_EQ(tree.result, tree.ops[0].id);
    ms_logical_tree_destroy(&tree);
}

static void test_logical_parse_precedence(void)
{
    ms_logical_tree_t tree;

    EXPECT_EQ(ms_logical_parse("(101|102)&!103", &tree), MS_SUCCESS);
    EXPECT_EQ(tree.op_count, 3U);
    EXPECT_EQ(tree.ops[0].op, (uint32_t)MS_LOGICAL_OP_OR);
    EXPECT_EQ(tree.ops[0].left, 101U);
    EXPECT_EQ(tree.ops[0].right, 102U);
    EXPECT_EQ(tree.ops[1].op, (uint32_t)MS_LOGICAL_OP_NOT);
    EXPECT_EQ(tree.ops[1].right, 103U);
    EXPECT_EQ(tree.ops[2].op, (uint32_t)MS_LOGICAL_OP_AND);
    EXPECT_EQ(tree.ops[2].left, tree.ops[0].id);
    EXPECT_EQ(tree.ops[2].right, tree.ops[1].id);
    EXPECT_EQ(tree.result, tree.ops[2].id);
    ms_logical_tree_destroy(&tree);
}

static void test_logical_parse_invalid(void)
{
    ms_logical_tree_t tree;

    EXPECT_EQ(ms_logical_parse("(101&102", &tree), MS_INVALID_PARA);
    EXPECT_EQ(ms_logical_parse("101&&102", &tree), MS_INVALID_PARA);
    EXPECT_EQ(ms_logical_parse("101#102", &tree), MS_INVALID_PARA);
    EXPECT_EQ(ms_logical_parse("101", &tree), MS_INVALID_PARA);
    EXPECT_EQ(ms_logical_parse(NULL, &tree), MS_INVALID_PARA);
    EXPECT_EQ(ms_logical_parse("101&102", NULL), MS_INVALID_PARA);
}

KTEST_ADD_TEST_CASE(SuiteLogicalParse, test_logical_parse_and);
KTEST_ADD_TEST_CASE(SuiteLogicalParse, test_logical_parse_not);
KTEST_ADD_TEST_CASE(SuiteLogicalParse, test_logical_parse_precedence);
KTEST_ADD_TEST_CASE(SuiteLogicalParse, test_logical_parse_invalid);

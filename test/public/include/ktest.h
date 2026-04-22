/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: KTest function
 */
#ifndef KTEST_H
#define KTEST_H
#include <mock.h>
#include <mock_func.h>

#ifdef __cplusplus
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define KTEST_ADD_TEST_CASE(suiteName, testCase) \
    TEST(suiteName, testCase)                    \
    {                                            \
        testCase();                              \
    }

#else
#include <register_cunit.h>
#define TEST_SUITE_REGISTER __attribute__((constructor))

#define EXPECT_TRUE(a) CU_ASSERT_TRUE(a)
#define EXPECT_FALSE(a) CU_ASSERT_FALSE(a)
#define EXPECT_EQ(a, b) CU_ASSERT_EQUAL(a, b)
#define EXPECT_NOT_EQ(a, b) CU_ASSERT_NOT_EQUAL(a, b)
#define EXPECT_STREQ(a, b) CU_ASSERT_STRING_EQUAL(a, b)
#define EXPECT_PTR_NULL(a, b) CU_ASSERT_PTR_NULL(a)
#define EXPECT_PTR_NOT_NULL(a, b) CU_ASSERT_PTR_NULL(a)

#define EXPECT_NE(a, b) CU_ASSERT_NOT_EQUAL(a, b)

#define KTEST_ADD_TEST_CASE(suiteName, testCase)         \
    void suiteName##testCase()                           \
    {                                                    \
        CUNIT_ADD_TEST_CASE(suiteName, testCase);        \
    }                                                    \
    TEST_SUITE_REGISTER void suiteName##testCase##test() \
    {                                                    \
        CunitAddSuite(suiteName##testCase);              \
    }
#endif

#endif

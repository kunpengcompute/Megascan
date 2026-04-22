/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: test interview function
 */
#include <ktest.h>
#include <stdio.h>
#include <unistd.h>

int return0()
{
    printf("return 0\n");
    return 0;
}

int returnf1()
{
    return -1;
}

void test_demo()
{
    /* 创建Mock结构 */
    KTestMock *Mock = CreateMock();

    int ret = return0();
    EXPECT_EQ(ret, 0);

    /* 创建Mock */
    Mock->Create(return0, returnf1);

    ret = return0();
    EXPECT_EQ(ret, -1);

    /* 去除Mock */
    Mock->Delete(return0);

    ret = return0();
    EXPECT_EQ(ret, 0);

    /* 销毁Mock结构 */
    DeleteMock(Mock);
}

void test_demo1()
{
    /* 创建Mock结构 */
    KTestMock *Mock = CreateMock();

    int ret = return0();
    EXPECT_EQ(ret, 0);

    /* 创建Mock */
    Mock->Create(return0, returnf1);

    ret = return0();
    EXPECT_EQ(ret, -1);

    /* 去除Mock */
    Mock->Delete(return0);

    ret = return0();
    EXPECT_EQ(ret, 0);

    /* 销毁Mock结构 */
    DeleteMock(Mock);
}

void test_demo2()
{
}

void test_demo3()
{
}

KTEST_ADD_TEST_CASE(SuiteKTestDemo, test_demo);
KTEST_ADD_TEST_CASE(SuiteKTestDemo, test_demo1);
KTEST_ADD_TEST_CASE(SuiteKTestDemo, test_demo2);
KTEST_ADD_TEST_CASE(SuiteKTestDemo1, test_demo3);
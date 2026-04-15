/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: test interview function
 */
#ifndef KTEST_INTERACTIVE_H
#define KTEST_INTERACTIVE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TEST_RUN_BEFORE_FUNC函数返回 */
typedef enum {
    TEST_RUN_ONE = 0,
    TEST_RUN_AGAIN,
    TEST_RUN_QUIT
} TEST_RET;

/* 测试框架注册函数修饰符 */
#define TEST_FUNC_REGISTER __attribute__((constructor))

typedef void (*TEST_START_LOG_FUNC)(void);

typedef TEST_RET (*TEST_RUN_BEFORE_FUNC)(int argc, char **argv, char **args);

typedef void (*TEST_RUN_FUNC)(int argc, char **argv, char *args);

typedef void (*TEST_RUN_AFTER_FUNC)(char *procName);

typedef void (*TEST_RUN_END_FUNC)(void);

typedef struct {
    bool registered;

    TEST_START_LOG_FUNC testStartLogFunc;

    TEST_RUN_BEFORE_FUNC testRunBeforeFunc;

    TEST_RUN_FUNC testRunFunc;

    TEST_RUN_AFTER_FUNC testRunAfterFunc;

    TEST_RUN_END_FUNC testRunEnd;
} InteractiveTest;

InteractiveTest *TestGetInterview(void);

/*****************************************************************************
 函 数 名  : registerTestFunc
 功能描述  : 注册测试框架参数，用于各测试框架注册自己的函数
 输入参数  : testStartLogFunc：测试启动时输出函数
             testRunBeforeFunc：测试用例运行前函数
             testRunFunc：测试用例执行函数
             testRunAfterFunc：测试用例运行后函数
 输出参数  : 无
*****************************************************************************/
void TestInterviewRegister(TEST_START_LOG_FUNC testStartLogFunc,
    TEST_RUN_BEFORE_FUNC testRunBeforeFunc, TEST_RUN_FUNC testRunFunc,
    TEST_RUN_AFTER_FUNC testRunAfterFunc, TEST_RUN_END_FUNC testRunEnd);

#ifdef __cplusplus
}
#endif
#endif
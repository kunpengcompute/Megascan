/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: test interview function
 */
#include "ktest_interactive.h"

#ifdef __cplusplus
extern "C" {
#endif

InteractiveTest g_interactiveTest = {false, NULL, NULL, NULL, NULL, NULL};


InteractiveTest *TestGetInterview(void)
{
    return &g_interactiveTest;
}

void TestInterviewRegister(TEST_START_LOG_FUNC testStartLogFunc,
    TEST_RUN_BEFORE_FUNC testRunBeforeFunc, TEST_RUN_FUNC testRunFunc,
    TEST_RUN_AFTER_FUNC testRunAfterFunc, TEST_RUN_END_FUNC testRunEnd)
{
    g_interactiveTest.testStartLogFunc = testStartLogFunc;
    g_interactiveTest.testRunBeforeFunc = testRunBeforeFunc;
    g_interactiveTest.testRunFunc = testRunFunc;
    g_interactiveTest.testRunAfterFunc = testRunAfterFunc;
    g_interactiveTest.testRunEnd = testRunEnd;
    g_interactiveTest.registered = (testStartLogFunc != NULL) && (testRunBeforeFunc != NULL) &&
        (testRunFunc != NULL) && (testRunAfterFunc != NULL) && (testRunEnd != NULL);
}

#ifdef __cplusplus
}
#endif
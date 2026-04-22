/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Rcunit regester function
 */
#include "register_cunit.h"
#include "ktest_interactive.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_CASE_MAXNUM 100

int g_suiteNum = 0;
RegisterSuiteFunc funcArr[sizeof(RegisterSuiteFunc) * TEST_CASE_MAXNUM] = {0};

void CunitEndFunc()
{
    printf("test end!\r\n");
}

void CunitRunAfterFunc(char *procName)
{
    CU_cleanup_registry();
    (void)CU_get_error();
}

void CunitTestRun(int argc, char **argv, char *args)
{
    if (CUE_SUCCESS != CU_initialize_registry()) {
        (void)CU_get_error();
        return;
    }

    for (int i = 0; i < g_suiteNum; i++) {
        RegisterSuiteFunc func = funcArr[i];
        func();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
}

TEST_RET CunitRunBeforeFunc(int argc, char **argv, char **args)
{
    return TEST_RUN_ONE;
}

void CunitStartLogFunc()
{
    printf("start Cunit test\r\n");
}

TEST_FUNC_REGISTER void RegisterTestFunc(void)
{
    TestInterviewRegister(CunitStartLogFunc, CunitRunBeforeFunc, CunitTestRun, CunitRunAfterFunc, CunitEndFunc);
}

void CunitAddSuite(RegisterSuiteFunc func)
{
    funcArr[g_suiteNum] = func;
    g_suiteNum++;
}

#ifdef __cplusplus
}
#endif
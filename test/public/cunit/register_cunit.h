/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: cunit regester function
 */
#ifndef REGISTER_CUNIT_H
#define REGISTER_CUNIT_H

#include <stdio.h>
#include <stdlib.h>
#include "CUnit/Basic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RegisterSuiteFunc)(void);

void CunitAddSuite(RegisterSuiteFunc func);

#define CUNIT_ADD_TEST_CASE(suiteName, testFunc) \
do { \
    CU_pSuite suite = CU_get_suite(# suiteName); \
    if (suite == NULL) { \
        suite = CU_add_suite(# suiteName, NULL, NULL); \
    } \
    CU_add_test(suite, # testFunc, (CU_TestFunc)(testFunc)); \
} while (0)

#ifdef __cplusplus
}
#endif

#endif
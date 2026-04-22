/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: test main function
 */
#include "ktest_interactive.h"

#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char **argv)
{
    InteractiveTest *interactiveTest = TestGetInterview();
    if (!interactiveTest->registered) {
        return -1;
    }
    interactiveTest->testStartLogFunc();

    char *pArg = NULL;
    TEST_RET ret = interactiveTest->testRunBeforeFunc(argc, argv, &pArg);

    while (ret != TEST_RUN_QUIT) {
        interactiveTest->testRunFunc(argc, argv, pArg);

        interactiveTest->testRunAfterFunc(argv[0]);
        if (ret == TEST_RUN_ONE) {
            break;
        }
        if (pArg != NULL) {
            free(pArg);
            pArg = NULL;
        }
        ret = interactiveTest->testRunBeforeFunc(argc, argv, &pArg);
    }
    interactiveTest->testRunEnd();
    return 0;
}
#ifdef __cplusplus
}
#endif
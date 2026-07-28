/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: hash compatibility API unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"
#include "../../../ms/include/ms_handler.h"
#include "../../../ms/include/ms_rule.h"

static void test_hash_add_rule_accepts_binary_key(void)
{
    ms_handler_t handler = {0};
    uint8_t key[16] = {0x12, 0x00, 0x34, 0x56, 0x00, 0x78, 0x9a, 0xbc,
                       0xde, 0xf0, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    ms_rule_t rule = {1001, sizeof(key), key};
    ms_rule_t query = {0, sizeof(key), key};

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 16, 8), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, &rule, 1), MS_SUCCESS);
    EXPECT_EQ(ms_find_rule(&handler, &query), MS_SUCCESS);
    EXPECT_EQ(query.alias, 1001U);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteHashCompat, test_hash_add_rule_accepts_binary_key);

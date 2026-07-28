/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: AC boolean fast path and callback reporting unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"

static int contains_id(const uint32_t *ids, uint32_t count, uint32_t id)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        if (ids[i] == id) {
            return 1;
        }
    }
    return 0;
}

static int compile_suffix_database(ms_database_t **db)
{
    const char *expressions[] = {"he", "she"};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_NONE};
    uint32_t ids[] = {101, 102};

    return ms_compile_database_multi(db, MS_MATCHER_TYPE_AC, 0, 0,
                                       expressions, flags, ids, 2);
}

static void test_ac_database_find_uses_boolean_match_path(void)
{
    ms_database_t *db = NULL;
    ms_rule_t matching = {
        .alias = 0,
        .key = (uint8_t *)"she",
        .key_len = 3,
    };
    ms_rule_t missing = {
        .alias = 0,
        .key = (uint8_t *)"xyz",
        .key_len = 3,
    };

    EXPECT_EQ(compile_suffix_database(&db), MS_SUCCESS);
    EXPECT_EQ(ms_find_database_rule(db, &matching), MS_SUCCESS);
    EXPECT_EQ(ms_find_database_rule(db, &missing), MS_FIND_FAIL);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_ac_database_scan_reports_all_suffix_ids(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    uint32_t out_ids[2] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(compile_suffix_database(&db), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);

    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"she", 3,
                                 scratch, out_ids, &out_count, 2), MS_SUCCESS);
    EXPECT_EQ(out_count, 2U);
    EXPECT_TRUE(contains_id(out_ids, out_count, 101));
    EXPECT_TRUE(contains_id(out_ids, out_count, 102));

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteAcMatch, test_ac_database_find_uses_boolean_match_path);
KTEST_ADD_TEST_CASE(SuiteAcMatch, test_ac_database_scan_reports_all_suffix_ids);

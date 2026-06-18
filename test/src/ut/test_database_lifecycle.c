/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: database lifecycle unit tests
 */
#include <stdint.h>
#include <string.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"

static mmpm_rule_t make_database_lifecycle_query(uint32_t alias, const char *s)
{
    mmpm_rule_t rule;

    rule.alias = alias;
    rule.key = (uint8_t *)s;
    rule.key_len = (uint32_t)strlen(s);

    return rule;
}

static void test_database_compile_hash_plain_and_find(void)
{
    mmpm_database_t *db = NULL;
    const char *expr[] = {"0123456789abcdef"};
    uint32_t ids[] = {2001};
    uint32_t flags[] = {MMPM_FLAG_NONE};
    mmpm_rule_t query = make_database_lifecycle_query(0, "0123456789abcdef");

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_HASH, 16, 8,
                                          expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_TRUE(db != NULL);
    EXPECT_EQ(mmpm_find_database_rule(db, &query), MMPM_SUCCESS);
    EXPECT_EQ(query.alias, 2001U);

    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

static void test_database_rejects_hash_combination(void)
{
    mmpm_database_t *db = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_NONE, MMPM_FLAG_COMBINATION};

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_HASH, 16, 8,
                                          expr, flags, ids, 3), MMPM_NOT_SUPPORTED);
    EXPECT_TRUE(db == NULL);
}

static void test_database_open_stream_rejects_hash(void)
{
    mmpm_database_t *db = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"0123456789abcdef"};
    uint32_t ids[] = {2001};
    uint32_t flags[] = {MMPM_FLAG_NONE};

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_HASH, 16, 8,
                                          expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(db, &stream), MMPM_INVALID_PARA);
    EXPECT_TRUE(stream == NULL);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

static void test_database_compile_ac_combination_and_scan(void)
{
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_TRUE(db != NULL);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);
    EXPECT_TRUE(scratch != NULL);
    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"abcdef", 6,
                                 scratch, out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteDatabaseLifecycle, test_database_compile_hash_plain_and_find);
KTEST_ADD_TEST_CASE(SuiteDatabaseLifecycle, test_database_rejects_hash_combination);
KTEST_ADD_TEST_CASE(SuiteDatabaseLifecycle, test_database_open_stream_rejects_hash);
KTEST_ADD_TEST_CASE(SuiteDatabaseLifecycle, test_database_compile_ac_combination_and_scan);

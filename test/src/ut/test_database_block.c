/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: database block scan unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"

static void test_database_scan_with_explicit_scratch(void)
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

static void test_database_scan_clears_scratch_per_block(void)
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
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);

    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"def", 3,
                                 scratch, out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);

    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"abcdef", 6,
                                 scratch, out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

static void test_database_scan_rejects_wrong_scratch(void)
{
    mmpm_database_t *db1 = NULL;
    mmpm_database_t *db2 = NULL;
    mmpm_scratch_t *scratch = NULL;
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MMPM_FLAG_NONE};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_compile_database_multi(&db1, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_database_multi(&db2, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db1, &scratch), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_database(db2, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, &out_count, 1), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db2), MMPM_SUCCESS);
}

static void test_database_block_invalid_parameters(void)
{
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MMPM_FLAG_NONE};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_alloc_scratch(NULL, &scratch), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_free_scratch(NULL), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_alloc_scratch(db, NULL), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_scan_database(NULL, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_database(db, NULL, 3,
                                 scratch, out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"abc", 3,
                                 NULL, out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, NULL, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, NULL, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, NULL, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, NULL, &out_count, 0), MMPM_INVALID_PARA);
    EXPECT_EQ(out_count, 1U);

    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteDatabaseBlock, test_database_scan_with_explicit_scratch);
KTEST_ADD_TEST_CASE(SuiteDatabaseBlock, test_database_scan_clears_scratch_per_block);
KTEST_ADD_TEST_CASE(SuiteDatabaseBlock, test_database_scan_rejects_wrong_scratch);
KTEST_ADD_TEST_CASE(SuiteDatabaseBlock, test_database_block_invalid_parameters);

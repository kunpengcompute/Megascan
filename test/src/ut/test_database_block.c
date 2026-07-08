/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: database block scan unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"

static void test_database_scan_with_explicit_scratch(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_TRUE(db != NULL);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_TRUE(scratch != NULL);

    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                                 scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_database_scan_clears_scratch_per_block(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);

    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);

    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"def", 3,
                                 scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);

    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                                 scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_database_scan_rejects_wrong_scratch(void)
{
    ms_database_t *db1 = NULL;
    ms_database_t *db2 = NULL;
    ms_scratch_t *scratch = NULL;
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MS_FLAG_NONE};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db1, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db2, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db1, &scratch), MS_SUCCESS);

    EXPECT_EQ(ms_scan_database(db2, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, &out_count, 1), MS_INVALID_PARA);

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db1), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db2), MS_SUCCESS);
}

static void test_database_block_invalid_parameters(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MS_FLAG_NONE};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_alloc_scratch(NULL, &scratch), MS_INVALID_PARA);
    EXPECT_EQ(ms_free_scratch(NULL), MS_INVALID_PARA);

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);

    EXPECT_EQ(ms_alloc_scratch(db, NULL), MS_INVALID_PARA);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_INVALID_PARA);

    EXPECT_EQ(ms_scan_database(NULL, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_database(db, NULL, 3,
                                 scratch, out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abc", 3,
                                 NULL, out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, NULL, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, NULL, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, NULL, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, NULL, &out_count, 0), MS_INVALID_PARA);
    EXPECT_EQ(out_count, 1U);

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteDatabaseBlock, test_database_scan_with_explicit_scratch);
KTEST_ADD_TEST_CASE(SuiteDatabaseBlock, test_database_scan_clears_scratch_per_block);
KTEST_ADD_TEST_CASE(SuiteDatabaseBlock, test_database_scan_rejects_wrong_scratch);
KTEST_ADD_TEST_CASE(SuiteDatabaseBlock, test_database_block_invalid_parameters);

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: database stream unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"
#include "../../../mmpm/include/mmpm_runtime.h"

static void test_database_open_stream_and_scan_combination(void)
{
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(db, &stream), MMPM_SUCCESS);
    EXPECT_TRUE(stream != NULL);

    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);

    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(mmpm_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

static void test_database_stream_uses_explicit_scratch(void)
{
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(db, &stream), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(mmpm_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

static void test_database_stream_close_reports_final_combination(void)
{
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "!101"};
    uint32_t ids[] = {101, 2001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(db, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"zzz", 3,
                               scratch, out_ids, &out_count, 1), MMPM_FIND_FAIL);

    EXPECT_EQ(mmpm_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 2001U);
    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

static void test_database_stream_rejects_wrong_scratch(void)
{
    mmpm_database_t *db1 = NULL;
    mmpm_database_t *db2 = NULL;
    mmpm_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MMPM_FLAG_NONE};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_compile_database_multi(&db1, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_database_multi(&db2, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db2, &scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(db1, &stream), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db2), MMPM_SUCCESS);
}

static void test_database_stream_invalid_parameters(void)
{
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MMPM_FLAG_NONE};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(mmpm_open_stream(NULL, &stream), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_stream(NULL, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_flush_stream(NULL, scratch, out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_close_stream(NULL, scratch, out_ids, &out_count, 1), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(db, &stream), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_stream(db, &stream), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_stream(stream, NULL, 3,
                               scratch, out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3,
                               NULL, out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, NULL, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, NULL, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_flush_stream(stream, NULL, out_ids, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_flush_stream(stream, scratch, NULL, &out_count, 1), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_close_stream(stream, NULL, out_ids, &out_count, 1), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_open_stream_and_scan_combination);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_uses_explicit_scratch);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_close_reports_final_combination);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_rejects_wrong_scratch);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_invalid_parameters);

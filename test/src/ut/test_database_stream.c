/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: database stream unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"
#include "../../../ms/include/ms_runtime.h"

static void test_database_open_stream_and_scan_combination(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &stream), MS_SUCCESS);
    EXPECT_TRUE(stream != NULL);

    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);

    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(ms_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_database_stream_uses_explicit_scratch(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &stream), MS_SUCCESS);

    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(ms_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 0U);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_database_stream_close_reports_final_combination(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "!101"};
    uint32_t ids[] = {101, 2001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"zzz", 3,
                               scratch, out_ids, &out_count, 1), MS_FIND_FAIL);

    EXPECT_EQ(ms_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 2001U);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_database_stream_rejects_wrong_scratch(void)
{
    ms_database_t *db1 = NULL;
    ms_database_t *db2 = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MS_FLAG_NONE};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db1, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db2, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db2, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db1, &stream), MS_SUCCESS);

    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MS_INVALID_PARA);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db1), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db2), MS_SUCCESS);
}

static void test_database_stream_rolls_back_failed_scan_and_resets_sparse_state(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &stream), MS_SUCCESS);

    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, NULL, &out_count, 0), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);

    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, out_ids, &out_count, 1), MS_FIND_FAIL);

    EXPECT_EQ(ms_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_database_stream_does_not_leak_interleaved_block_state(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &stream), MS_SUCCESS);

    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abc", 3,
                                 scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"def", 3,
                                 scratch, out_ids, &out_count, 1), MS_FIND_FAIL);

    EXPECT_EQ(ms_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_database_stream_invalid_parameters(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc"};
    uint32_t ids[] = {101};
    uint32_t flags[] = {MS_FLAG_NONE};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    EXPECT_EQ(ms_open_stream(NULL, &stream), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_stream(NULL, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_flush_stream(NULL, scratch, out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_close_stream(NULL, scratch, out_ids, &out_count, 1), MS_INVALID_PARA);

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &stream), MS_SUCCESS);

    EXPECT_EQ(ms_open_stream(db, &stream), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_stream(stream, NULL, 3,
                               scratch, out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               NULL, out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, NULL, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, NULL, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_flush_stream(stream, NULL, out_ids, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_flush_stream(stream, scratch, NULL, &out_count, 1), MS_INVALID_PARA);
    EXPECT_EQ(ms_close_stream(stream, NULL, out_ids, &out_count, 1), MS_INVALID_PARA);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_open_stream_and_scan_combination);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_uses_explicit_scratch);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_close_reports_final_combination);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_rejects_wrong_scratch);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_rolls_back_failed_scan_and_resets_sparse_state);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_does_not_leak_interleaved_block_state);
KTEST_ADD_TEST_CASE(SuiteDatabaseStream, test_database_stream_invalid_parameters);

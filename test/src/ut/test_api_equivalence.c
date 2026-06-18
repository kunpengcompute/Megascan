/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: full API and compatibility API equivalence unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"
#include "../../../mmpm/include/mmpm_handler.h"
#include "../../../mmpm/include/mmpm_compile.h"
#include "../../../mmpm/include/mmpm_runtime.h"
#include "../../../mmpm/include/mmpm_expr.h"

static void test_api_equivalence_block_scan_matches_database_and_handler(void)
{
    mmpm_handler_t handler = {0};
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t handler_ids[2] = {0};
    uint32_t handler_count = 0;
    uint32_t db_ids[2] = {0};
    uint32_t db_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_expr(&handler, (const uint8_t *)"xxabcdefyy", 10,
                             handler_ids, &handler_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_database(db, (const uint8_t *)"xxabcdefyy", 10,
                                 scratch, db_ids, &db_count, 2), MMPM_SUCCESS);

    EXPECT_EQ(handler_count, db_count);
    EXPECT_EQ(handler_ids[0], db_ids[0]);
    EXPECT_EQ(handler_ids[1], db_ids[1]);

    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_api_equivalence_combination_eval_matches_scan_result(void)
{
    mmpm_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t aliases[] = {101, 102};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;
    uint32_t scan_comb_ids[1] = {0};
    uint32_t scan_comb_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_compile_eval_combinations(&handler, aliases, 2,
                                             comb_ids, &comb_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcdefyy", 10,
                                                scan_comb_ids, &scan_comb_count, 1), MMPM_SUCCESS);

    EXPECT_EQ(comb_count, scan_comb_count);
    EXPECT_EQ(comb_ids[0], scan_comb_ids[0]);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_api_equivalence_stream_matches_database_and_handler(void)
{
    mmpm_handler_t handler = {0};
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    ms_stream_t *handler_stream = NULL;
    ms_stream_t *db_stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_NONE, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t handler_ids[2] = {0};
    uint32_t handler_count = 0;
    uint32_t db_ids[2] = {0};
    uint32_t db_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_handler_stream(&handler, &handler_stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(db, &db_stream), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_expr_stream(handler_stream, (const uint8_t *)"xxabc", 5,
                                    handler_ids, &handler_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_stream(db_stream, (const uint8_t *)"xxabc", 5,
                               scratch, db_ids, &db_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(handler_count, db_count);
    EXPECT_EQ(handler_ids[0], db_ids[0]);

    handler_ids[0] = handler_ids[1] = 0;
    db_ids[0] = db_ids[1] = 0;
    handler_count = 0;
    db_count = 0;

    EXPECT_EQ(mmpm_scan_expr_stream(handler_stream, (const uint8_t *)"yydef", 5,
                                    handler_ids, &handler_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_stream(db_stream, (const uint8_t *)"yydef", 5,
                               scratch, db_ids, &db_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(handler_count, db_count);
    EXPECT_EQ(handler_ids[0], db_ids[0]);

    EXPECT_EQ(mmpm_close_handler_stream(handler_stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_close_stream(db_stream, scratch, db_ids, &db_count, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_api_equivalence_stream_flush_matches_database_and_handler(void)
{
    mmpm_handler_t handler = {0};
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    ms_stream_t *handler_stream = NULL;
    ms_stream_t *db_stream = NULL;
    const char *expr[] = {"abc", "!101"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};
    uint32_t handler_ids[1] = {0};
    uint32_t handler_count = 0;
    uint32_t db_ids[1] = {0};
    uint32_t db_count = 0;

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_handler_stream(&handler, &handler_stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(db, &db_stream), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_expr_stream(handler_stream, (const uint8_t *)"zzz", 3,
                                    handler_ids, &handler_count, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_stream(db_stream, (const uint8_t *)"zzz", 3,
                               scratch, db_ids, &db_count, 1), MMPM_FIND_FAIL);

    EXPECT_EQ(mmpm_scan_expr_stream_flush(handler_stream,
                                          handler_ids, &handler_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_flush_stream(db_stream, scratch,
                                db_ids, &db_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(handler_count, db_count);
    EXPECT_EQ(handler_ids[0], db_ids[0]);

    EXPECT_EQ(mmpm_close_handler_stream(handler_stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_close_stream(db_stream, scratch, db_ids, &db_count, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_block_scan_matches_database_and_handler);
KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_combination_eval_matches_scan_result);
KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_stream_matches_database_and_handler);
KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_stream_flush_matches_database_and_handler);

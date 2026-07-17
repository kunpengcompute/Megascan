/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: full API and compatibility API equivalence unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"
#include "../../../ms/include/ms_handler.h"
#include "../../../ms/include/ms_compile.h"
#include "../../../ms/include/ms_runtime.h"
#include "../../../ms/include/ms_expr.h"

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

static void expect_same_id_set(const uint32_t *lhs, uint32_t lhs_count,
                               const uint32_t *rhs, uint32_t rhs_count)
{
    uint32_t i;

    EXPECT_EQ(lhs_count, rhs_count);
    for (i = 0; i < lhs_count; i++) {
        EXPECT_TRUE(contains_id(rhs, rhs_count, lhs[i]));
    }
}

static void test_api_equivalence_block_scan_matches_database_and_handler(void)
{
    ms_handler_t handler = {0};
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t handler_ids[2] = {0};
    uint32_t handler_count = 0;
    uint32_t db_ids[2] = {0};
    uint32_t db_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);

    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxabcdefyy", 10,
                             handler_ids, &handler_count, 2), MS_SUCCESS);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"xxabcdefyy", 10,
                                 scratch, db_ids, &db_count, 2), MS_SUCCESS);

    EXPECT_EQ(handler_count, db_count);
    EXPECT_EQ(handler_ids[0], db_ids[0]);
    EXPECT_EQ(handler_ids[1], db_ids[1]);

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_api_equivalence_combination_eval_matches_scan_result(void)
{
    ms_handler_t handler = {0};
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t aliases[] = {101, 102};
    uint32_t comb_ids[1] = {0};
    uint32_t comb_count = 0;
    uint32_t scan_comb_ids[1] = {0};
    uint32_t scan_comb_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);

    EXPECT_EQ(ms_compile_eval_combinations(&handler, aliases, 2,
                                             comb_ids, &comb_count, 1), MS_SUCCESS);
    EXPECT_EQ(ms_compile_scan_ac_combinations(&handler, (const uint8_t *)"xxabcdefyy", 10,
                                                scan_comb_ids, &scan_comb_count, 1), MS_SUCCESS);

    EXPECT_EQ(comb_count, scan_comb_count);
    EXPECT_EQ(comb_ids[0], scan_comb_ids[0]);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_api_equivalence_stream_matches_database_and_handler(void)
{
    ms_handler_t handler = {0};
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *handler_stream = NULL;
    ms_stream_t *db_stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t handler_ids[2] = {0};
    uint32_t handler_count = 0;
    uint32_t db_ids[2] = {0};
    uint32_t db_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &handler_stream), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &db_stream), MS_SUCCESS);

    EXPECT_EQ(ms_scan_expr_stream(handler_stream, (const uint8_t *)"xxabc", 5,
                                    handler_ids, &handler_count, 2), MS_SUCCESS);
    EXPECT_EQ(ms_scan_stream(db_stream, (const uint8_t *)"xxabc", 5,
                               scratch, db_ids, &db_count, 2), MS_SUCCESS);
    EXPECT_EQ(handler_count, db_count);
    EXPECT_EQ(handler_ids[0], db_ids[0]);

    handler_ids[0] = handler_ids[1] = 0;
    db_ids[0] = db_ids[1] = 0;
    handler_count = 0;
    db_count = 0;

    EXPECT_EQ(ms_scan_expr_stream(handler_stream, (const uint8_t *)"yydef", 5,
                                    handler_ids, &handler_count, 2), MS_SUCCESS);
    EXPECT_EQ(ms_scan_stream(db_stream, (const uint8_t *)"yydef", 5,
                               scratch, db_ids, &db_count, 2), MS_SUCCESS);
    EXPECT_EQ(handler_count, db_count);
    EXPECT_EQ(handler_ids[0], db_ids[0]);

    EXPECT_EQ(ms_close_handler_stream(handler_stream), MS_SUCCESS);
    EXPECT_EQ(ms_close_stream(db_stream, scratch, db_ids, &db_count, 2), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_api_equivalence_stream_flush_matches_database_and_handler(void)
{
    ms_handler_t handler = {0};
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *handler_stream = NULL;
    ms_stream_t *db_stream = NULL;
    const char *expr[] = {"abc", "!101"};
    uint32_t ids[] = {101, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t handler_ids[1] = {0};
    uint32_t handler_count = 0;
    uint32_t db_ids[1] = {0};
    uint32_t db_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, flags, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &handler_stream), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &db_stream), MS_SUCCESS);

    EXPECT_EQ(ms_scan_expr_stream(handler_stream, (const uint8_t *)"zzz", 3,
                                    handler_ids, &handler_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_scan_stream(db_stream, (const uint8_t *)"zzz", 3,
                               scratch, db_ids, &db_count, 1), MS_FIND_FAIL);

    EXPECT_EQ(ms_scan_expr_stream_flush(handler_stream,
                                          handler_ids, &handler_count, 1), MS_SUCCESS);
    EXPECT_EQ(ms_flush_stream(db_stream, scratch,
                                db_ids, &db_count, 1), MS_SUCCESS);
    EXPECT_EQ(handler_count, db_count);
    EXPECT_EQ(handler_ids[0], db_ids[0]);

    EXPECT_EQ(ms_close_handler_stream(handler_stream), MS_SUCCESS);
    EXPECT_EQ(ms_close_stream(db_stream, scratch, db_ids, &db_count, 1), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_api_equivalence_compact_ac_suffix_and_duplicate_literals(void)
{
    ms_handler_t handler = {0};
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    const char *expr[] = {"a", "ba", "cba", "ba", "xyz"};
    uint32_t ids[] = {101, 102, 103, 104, 105};
    uint32_t handler_ids[5] = {0};
    uint32_t handler_count = 0;
    uint32_t db_ids[5] = {0};
    uint32_t db_count = 0;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, NULL, ids, 5), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, NULL, ids, 5), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);

    EXPECT_EQ(ms_scan_expr(&handler, (const uint8_t *)"xxcbaxyz", 8,
                             handler_ids, &handler_count, 5), MS_SUCCESS);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"xxcbaxyz", 8,
                                 scratch, db_ids, &db_count, 5), MS_SUCCESS);
    expect_same_id_set(handler_ids, handler_count, db_ids, db_count);

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_api_equivalence_compact_ac_stream_suffix_boundary(void)
{
    ms_handler_t handler = {0};
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *handler_stream = NULL;
    ms_stream_t *db_stream = NULL;
    const char *expr[] = {"a", "ba", "cba", "ba", "xyz"};
    const char *chunks[] = {"xxc", "ba", "xyz"};
    uint32_t chunk_lengths[] = {3, 2, 3};
    uint32_t ids[] = {101, 102, 103, 104, 105};
    uint32_t handler_ids[5] = {0};
    uint32_t db_ids[5] = {0};
    uint32_t handler_count;
    uint32_t db_count;
    uint32_t i;

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_compile_multi(&handler, expr, NULL, ids, 5), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, NULL, ids, 5), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &handler_stream), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &db_stream), MS_SUCCESS);

    for (i = 0; i < 3; i++) {
        handler_count = 0;
        db_count = 0;
        EXPECT_EQ(ms_scan_expr_stream(handler_stream,
                                        (const uint8_t *)chunks[i],
                                        chunk_lengths[i], handler_ids,
                                        &handler_count, 5),
                  i == 0 ? MS_FIND_FAIL : MS_SUCCESS);
        EXPECT_EQ(ms_scan_stream(db_stream, (const uint8_t *)chunks[i],
                                   chunk_lengths[i], scratch, db_ids,
                                   &db_count, 5),
                  i == 0 ? MS_FIND_FAIL : MS_SUCCESS);
        expect_same_id_set(handler_ids, handler_count, db_ids, db_count);
    }

    EXPECT_EQ(ms_close_handler_stream(handler_stream), MS_SUCCESS);
    EXPECT_EQ(ms_close_stream(db_stream, scratch, db_ids, &db_count, 5), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_block_scan_matches_database_and_handler);
KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_combination_eval_matches_scan_result);
KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_stream_matches_database_and_handler);
KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_stream_flush_matches_database_and_handler);
KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_compact_ac_suffix_and_duplicate_literals);
KTEST_ADD_TEST_CASE(SuiteApiEquivalence, test_api_equivalence_compact_ac_stream_suffix_boundary);

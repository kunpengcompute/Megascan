/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: match event unit tests
 */
#include <stdint.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"

static void test_block_scan_reports_every_plain_match_position(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    const char *expr[] = {"aba", "bab"};
    uint32_t ids[] = {101, 102};
    uint32_t flags[] = {MS_FLAG_NONE, MS_FLAG_QUIET};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;
    ms_match_event_t events[4] = {0};
    uint32_t event_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 2), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_get_match_events(scratch, events, &event_count, 4),
              MS_INVALID_PARA);

    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"ababababa", 9,
                                 scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(ms_get_match_events(scratch, NULL, &event_count, 0), MS_SUCCESS);
    EXPECT_EQ(event_count, 4U);
    EXPECT_EQ(ms_get_match_events(scratch, events, &event_count, 3),
              MS_INVALID_PARA);
    EXPECT_EQ(event_count, 4U);
    EXPECT_EQ(ms_get_match_events(scratch, events, &event_count, 4), MS_SUCCESS);
    EXPECT_EQ(event_count, 4U);
    EXPECT_EQ(events[0].id, 101U);
    EXPECT_EQ(events[0].from, 0U);
    EXPECT_EQ(events[0].to, 3U);
    EXPECT_EQ(events[1].from, 2U);
    EXPECT_EQ(events[1].to, 5U);
    EXPECT_EQ(events[2].from, 4U);
    EXPECT_EQ(events[2].to, 7U);
    EXPECT_EQ(events[3].from, 6U);
    EXPECT_EQ(events[3].to, 9U);

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

static void test_stream_scan_reports_cross_chunk_global_position(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abcdef"};
    uint32_t ids[] = {101};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;
    ms_match_event_t event = {0};
    uint32_t event_count = 0;

    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, NULL, ids, 1), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &stream), MS_SUCCESS);

    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abc", 3,
                               scratch, out_ids, &out_count, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_get_match_events(scratch, NULL, &event_count, 0), MS_SUCCESS);
    EXPECT_EQ(event_count, 0U);

    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"def", 3,
                               scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 101U);
    EXPECT_EQ(ms_get_match_events(scratch, &event, &event_count, 1), MS_SUCCESS);
    EXPECT_EQ(event_count, 1U);
    EXPECT_EQ(event.id, 101U);
    EXPECT_EQ(event.from, 0U);
    EXPECT_EQ(event.to, 6U);

    EXPECT_EQ(ms_close_stream(stream, scratch,
                                out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteMatchEvents,
                    test_block_scan_reports_every_plain_match_position);
KTEST_ADD_TEST_CASE(SuiteMatchEvents,
                    test_stream_scan_reports_cross_chunk_global_position);

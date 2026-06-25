/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: allocator runtime reuse unit tests
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ktest.h"
#include "../../../ms/include/ms.h"
#include "../../../ms/include/ms_runtime.h"

typedef struct {
    uint32_t alloc_count;
    uint32_t calloc_count;
    uint32_t free_count;
} allocator_counter_t;

static allocator_counter_t g_db_counter;
static allocator_counter_t g_scratch_counter;
static allocator_counter_t g_stream_counter;

static void reset_counter(allocator_counter_t *counter)
{
    memset(counter, 0, sizeof(*counter));
}

static void *counter_alloc(allocator_counter_t *counter, size_t size)
{
    counter->alloc_count++;
    return malloc(size);
}

static void *counter_calloc(allocator_counter_t *counter, size_t nmemb, size_t size)
{
    counter->calloc_count++;
    return calloc(nmemb, size);
}

static void counter_free(allocator_counter_t *counter, void *ptr)
{
    counter->free_count++;
    free(ptr);
}

static void *db_alloc(size_t size)
{
    return counter_alloc(&g_db_counter, size);
}

static void *db_calloc(size_t nmemb, size_t size)
{
    return counter_calloc(&g_db_counter, nmemb, size);
}

static void db_free(void *ptr)
{
    counter_free(&g_db_counter, ptr);
}

static void *scratch_alloc(size_t size)
{
    return counter_alloc(&g_scratch_counter, size);
}

static void *scratch_calloc(size_t nmemb, size_t size)
{
    return counter_calloc(&g_scratch_counter, nmemb, size);
}

static void scratch_free(void *ptr)
{
    counter_free(&g_scratch_counter, ptr);
}

static void *stream_alloc(size_t size)
{
    return counter_alloc(&g_stream_counter, size);
}

static void *stream_calloc(size_t nmemb, size_t size)
{
    return counter_calloc(&g_stream_counter, nmemb, size);
}

static void stream_free(void *ptr)
{
    counter_free(&g_stream_counter, ptr);
}

static void reset_allocators(void)
{
    (void)ms_set_allocator(NULL, NULL, NULL);
}

static void reset_all_counters(void)
{
    reset_counter(&g_db_counter);
    reset_counter(&g_scratch_counter);
    reset_counter(&g_stream_counter);
}

static void test_allocator_database_scan_reuses_scratch_workspace(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    reset_all_counters();
    reset_allocators();
    EXPECT_EQ(ms_set_database_allocator(db_alloc, db_calloc, db_free), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_set_scratch_allocator(scratch_alloc, scratch_calloc, scratch_free), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);

    reset_counter(&g_db_counter);
    reset_counter(&g_scratch_counter);
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                                 scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(g_db_counter.alloc_count + g_db_counter.calloc_count, 0U);
    EXPECT_EQ(g_db_counter.free_count, 0U);
    EXPECT_NE(g_scratch_counter.calloc_count, 0U);

    reset_counter(&g_db_counter);
    reset_counter(&g_scratch_counter);
    out_ids[0] = 0;
    out_count = 0;
    EXPECT_EQ(ms_scan_database(db, (const uint8_t *)"abcdef", 6,
                                 scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(g_db_counter.alloc_count + g_db_counter.calloc_count, 0U);
    EXPECT_EQ(g_db_counter.free_count, 0U);
    EXPECT_EQ(g_scratch_counter.alloc_count + g_scratch_counter.calloc_count, 0U);
    EXPECT_EQ(g_scratch_counter.free_count, 0U);

    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
    reset_allocators();
}

static void test_allocator_database_stream_scan_reuses_scratch(void)
{
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MS_FLAG_QUIET, MS_FLAG_QUIET, MS_FLAG_COMBINATION};
    uint32_t out_ids[1] = {0};
    uint32_t out_count = 0;

    reset_all_counters();
    reset_allocators();
    EXPECT_EQ(ms_set_database_allocator(db_alloc, db_calloc, db_free), MS_SUCCESS);
    EXPECT_EQ(ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MS_SUCCESS);
    EXPECT_EQ(ms_set_scratch_allocator(scratch_alloc, scratch_calloc, scratch_free), MS_SUCCESS);
    EXPECT_EQ(ms_set_stream_allocator(stream_alloc, stream_calloc, stream_free), MS_SUCCESS);
    EXPECT_EQ(ms_alloc_scratch(db, &scratch), MS_SUCCESS);
    EXPECT_EQ(ms_open_stream(db, &stream), MS_SUCCESS);

    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abcdef", 6,
                               scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);

    reset_counter(&g_db_counter);
    reset_counter(&g_scratch_counter);
    reset_counter(&g_stream_counter);
    out_ids[0] = 0;
    out_count = 0;
    EXPECT_EQ(ms_scan_stream(stream, (const uint8_t *)"abcdef", 6,
                               scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(out_count, 1U);
    EXPECT_EQ(out_ids[0], 1001U);
    EXPECT_EQ(g_db_counter.alloc_count + g_db_counter.calloc_count, 0U);
    EXPECT_EQ(g_db_counter.free_count, 0U);
    EXPECT_EQ(g_scratch_counter.alloc_count + g_scratch_counter.calloc_count, 0U);
    EXPECT_EQ(g_scratch_counter.free_count, 0U);
    EXPECT_EQ(g_stream_counter.alloc_count + g_stream_counter.calloc_count, 0U);
    EXPECT_EQ(g_stream_counter.free_count, 0U);

    EXPECT_EQ(ms_close_stream(stream, scratch, out_ids, &out_count, 1), MS_SUCCESS);
    EXPECT_EQ(ms_free_scratch(scratch), MS_SUCCESS);
    EXPECT_EQ(ms_free_database(db), MS_SUCCESS);
    reset_allocators();
}

KTEST_ADD_TEST_CASE(SuiteAllocatorRuntimeReuse, test_allocator_database_scan_reuses_scratch_workspace);
KTEST_ADD_TEST_CASE(SuiteAllocatorRuntimeReuse, test_allocator_database_stream_scan_reuses_scratch);

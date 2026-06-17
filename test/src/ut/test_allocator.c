/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: allocator unit tests
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"

typedef struct {
    uint32_t alloc_count;
    uint32_t calloc_count;
    uint32_t free_count;
} allocator_counter_t;

static allocator_counter_t g_db_counter;
static allocator_counter_t g_scratch_counter;
static allocator_counter_t g_stream_counter;
static allocator_counter_t g_all_counter;

static void reset_counter(allocator_counter_t *counter)
{
    memset(counter, 0, sizeof(*counter));
}

static void reset_all_counters(void)
{
    reset_counter(&g_db_counter);
    reset_counter(&g_scratch_counter);
    reset_counter(&g_stream_counter);
    reset_counter(&g_all_counter);
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

static void *all_alloc(size_t size)
{
    return counter_alloc(&g_all_counter, size);
}

static void *all_calloc(size_t nmemb, size_t size)
{
    return counter_calloc(&g_all_counter, nmemb, size);
}

static void all_free(void *ptr)
{
    counter_free(&g_all_counter, ptr);
}

static mmpm_rule_t make_rule(uint32_t alias, const char *s)
{
    mmpm_rule_t rule;

    rule.alias = alias;
    rule.key = (uint8_t *)s;
    rule.key_len = (uint32_t)strlen(s);

    return rule;
}

static void reset_allocators(void)
{
    (void)mmpm_set_allocator(NULL, NULL, NULL);
}

static void test_allocator_set_all_covers_database_and_stream_paths(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};

    reset_all_counters();
    EXPECT_EQ(mmpm_set_allocator(all_alloc, all_calloc, all_free), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);

    EXPECT_NE(g_all_counter.alloc_count + g_all_counter.calloc_count, 0U);
    EXPECT_NE(g_all_counter.free_count, 0U);
    reset_allocators();
}

static void test_allocator_hash_uses_database_allocator(void)
{
    mmpm_handler_t handler = {0};
    mmpm_rule_t rule = make_rule(1001, "0123456789abcdef");

    reset_all_counters();
    EXPECT_EQ(mmpm_set_database_allocator(db_alloc, db_calloc, db_free), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 16, 8), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, &rule, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);

    EXPECT_NE(g_db_counter.alloc_count + g_db_counter.calloc_count, 0U);
    EXPECT_NE(g_db_counter.free_count, 0U);
    EXPECT_EQ(g_stream_counter.alloc_count + g_stream_counter.calloc_count, 0U);
    reset_allocators();
}

static void test_allocator_stream_uses_stream_allocator(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};

    reset_all_counters();
    reset_allocators();
    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_compile_multi(&handler, expr, flags, ids, 3), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_set_stream_allocator(stream_alloc, stream_calloc, stream_free), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_NE(g_stream_counter.calloc_count, 0U);
    EXPECT_NE(g_stream_counter.free_count, 0U);

    reset_allocators();
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_allocator_database_lifecycle_uses_database_allocator(void)
{
    mmpm_database_t *db = NULL;

    reset_all_counters();
    EXPECT_EQ(mmpm_set_database_allocator(db_alloc, db_calloc, db_free), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_create_database(&db, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_TRUE(db != NULL);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);

    EXPECT_NE(g_db_counter.alloc_count + g_db_counter.calloc_count, 0U);
    EXPECT_NE(g_db_counter.free_count, 0U);
    reset_allocators();
}

static void test_allocator_scratch_uses_scratch_allocator(void)
{
    mmpm_database_t *db = NULL;
    mmpm_scratch_t *scratch = NULL;
    const char *expr[] = {"abc", "def", "101&102"};
    uint32_t ids[] = {101, 102, 1001};
    uint32_t flags[] = {MMPM_FLAG_QUIET, MMPM_FLAG_QUIET, MMPM_FLAG_COMBINATION};

    reset_all_counters();
    reset_allocators();
    EXPECT_EQ(mmpm_compile_database_multi(&db, MMPM_MATCHER_TYPE_AC, 0, 0,
                                          expr, flags, ids, 3), MMPM_SUCCESS);
    EXPECT_TRUE(db != NULL);

    EXPECT_EQ(mmpm_set_scratch_allocator(scratch_alloc, scratch_calloc, scratch_free), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_alloc_scratch(db, &scratch), MMPM_SUCCESS);
    EXPECT_TRUE(scratch != NULL);
    EXPECT_EQ(mmpm_free_scratch(scratch), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_free_database(db), MMPM_SUCCESS);

    EXPECT_NE(g_scratch_counter.calloc_count, 0U);
    EXPECT_NE(g_scratch_counter.free_count, 0U);
    EXPECT_EQ(g_stream_counter.alloc_count + g_stream_counter.calloc_count, 0U);
    reset_allocators();
}

KTEST_ADD_TEST_CASE(SuiteAllocator, test_allocator_set_all_covers_database_and_stream_paths);
KTEST_ADD_TEST_CASE(SuiteAllocator, test_allocator_hash_uses_database_allocator);
KTEST_ADD_TEST_CASE(SuiteAllocator, test_allocator_stream_uses_stream_allocator);
KTEST_ADD_TEST_CASE(SuiteAllocator, test_allocator_database_lifecycle_uses_database_allocator);
KTEST_ADD_TEST_CASE(SuiteAllocator, test_allocator_scratch_uses_scratch_allocator);

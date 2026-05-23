/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: AC stream unit tests
 */
#include <string.h>
#include <stdint.h>
#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"

static mmpm_rule_t make_rule(uint32_t alias, const char *s)
{
    mmpm_rule_t rule;

    rule.alias = alias;
    rule.key = (uint8_t *)s;
    rule.key_len = (uint32_t)strlen(s);

    return rule;
}

static void test_ac_stream_cross_chunk_match(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(1001, "12345"),
    };

    mmpm_rule_t block_query1 = make_rule(0, "xx123");
    mmpm_rule_t block_query2 = make_rule(0, "45yy");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);

    /* Block mode cannot match across two independent buffers. */
    EXPECT_EQ(mmpm_find_rule(&handler, &block_query1), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_find_rule(&handler, &block_query2), MMPM_FIND_FAIL);

    /* Stream mode keeps AC state between chunks. */
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"xx123", 5), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"45yy", 4), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_independent_streams(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream1 = NULL;
    ms_stream_t *stream2 = NULL;

    mmpm_rule_t rules[] = {
        make_rule(2001, "abcdef"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_stream(&handler, &stream1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream2), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_stream(stream1, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_stream(stream2, (const uint8_t *)"xxx", 3), MMPM_FIND_FAIL);

    /* stream1 continues from "abc", so "def" completes the rule. */
    EXPECT_EQ(mmpm_scan_stream(stream1, (const uint8_t *)"def", 3), MMPM_SUCCESS);

    /* stream2 is independent and should not inherit stream1's state. */
    EXPECT_EQ(mmpm_scan_stream(stream2, (const uint8_t *)"def", 3), MMPM_FIND_FAIL);

    EXPECT_EQ(mmpm_close_stream(stream1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_close_stream(stream2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_reset(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(3001, "abcdef"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);

    /*
     * After reset, the stream no longer remembers "abc",
     * so scanning "def" alone should not match "abcdef".
     */
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"def", 3), MMPM_FIND_FAIL);

    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_stale_after_remove(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(4001, "abcde"),
        make_rule(4002, "xyz"),
    };

    mmpm_rule_t remove_rule[] = {
        make_rule(4001, "abcde"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 2), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);

    /*
     * Removing rules rebuilds the AC table and reindexes states.
     * The old stream state must be treated as stale.
     */
    EXPECT_EQ(mmpm_remove_rule(&handler, remove_rule, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"de", 2), MMPM_STREAM_STALE);

    /*
     * reset_stream refreshes generation and returns to root state.
     * The removed rule should no longer match.
     */
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"abcde", 5), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_scan_stream(stream, (const uint8_t *)"xyz", 3), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_invalid_usage(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(5001, "abc"),
    };

    EXPECT_EQ(mmpm_open_stream(NULL, &stream), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_open_stream(&handler, NULL), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);

    /* Cannot open stream before AC rules are built. */
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_stream(&handler, &stream), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_scan_stream(NULL, (const uint8_t *)"abc", 3), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_scan_stream(stream, NULL, 3), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_close_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_cross_chunk_match);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_independent_streams);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_reset);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_stale_after_remove);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_invalid_usage);
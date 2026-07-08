/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: legacy AC rule stream compatibility unit tests
 */
#include <string.h>
#include <stdint.h>
#include "ktest.h"
#include "../../../ms/include/ms.h"
#include "../../../ms/include/ms_handler.h"
#include "../../../ms/include/ms_compile.h"
#include "../../../ms/include/ms_rule.h"
#include "../../../ms/include/ms_runtime.h"

static ms_rule_t make_rule(uint32_t alias, const char *s)
{
    ms_rule_t rule;

    rule.alias = alias;
    rule.key = (uint8_t *)s;
    rule.key_len = (uint32_t)strlen(s);

    return rule;
}

static int scan_stream_raw(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    return ms_scan_rule_stream(stream, data, len);
}

static int scan_stream_by_blocks(ms_handler_t *handler, const char *data, uint32_t stream_blocks)
{
    ms_stream_t *stream = NULL;
    uint32_t len = (uint32_t)strlen(data);
    uint32_t block_size;
    uint32_t pos = 0;
    int final_ret = MS_FIND_FAIL;
    int ret;

    if (stream_blocks == 0) {
        return MS_INVALID_PARA;
    }

    ret = ms_open_handler_stream(handler, &stream);
    EXPECT_EQ(ret, MS_SUCCESS);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    block_size = len / stream_blocks;
    if (block_size == 0) {
        block_size = 1;
    }

    while (pos < len) {
        uint32_t cur = block_size;
        if (pos + cur > len) {
            cur = len - pos;
        }

        ret = scan_stream_raw(stream, (const uint8_t *)(data + pos), cur);
        if (ret == MS_SUCCESS) {
            final_ret = MS_SUCCESS;
        } else if (ret != MS_FIND_FAIL) {
            final_ret = ret;
            break;
        }

        pos += cur;
    }

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    return final_ret;
}

static void test_ac_stream_cross_chunk_match(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    ms_rule_t rules[] = {
        make_rule(1001, "12345"),
    };

    ms_rule_t block_query1 = make_rule(0, "xx123");
    ms_rule_t block_query2 = make_rule(0, "45yy");

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 1), MS_SUCCESS);

    /* Block mode cannot match across two independent buffers. */
    EXPECT_EQ(ms_find_rule(&handler, &block_query1), MS_FIND_FAIL);
    EXPECT_EQ(ms_find_rule(&handler, &block_query2), MS_FIND_FAIL);

    /* Stream mode keeps AC state between chunks. */
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"xx123", 5), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"45yy", 4), MS_SUCCESS);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_hscollider_style_splits(void)
{
    ms_handler_t handler = {0};
    ms_rule_t rules[] = {
        make_rule(1101, "12345"),
    };
    ms_rule_t full_query = make_rule(0, "xx12345yy");

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 1), MS_SUCCESS);

    EXPECT_EQ(ms_find_rule(&handler, &full_query), MS_SUCCESS);

    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 1), MS_SUCCESS);
    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 2), MS_SUCCESS);
    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 3), MS_SUCCESS);
    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 5), MS_SUCCESS);
    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 20), MS_SUCCESS);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_multi_chunk_match(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    ms_rule_t rules[] = {
        make_rule(1201, "abcdef"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 1), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"ab", 2), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"cd", 2), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"ef", 2), MS_SUCCESS);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_independent_streams(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream1 = NULL;
    ms_stream_t *stream2 = NULL;

    ms_rule_t rules[] = {
        make_rule(2001, "abcdef"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 1), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream1), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream2), MS_SUCCESS);

    EXPECT_EQ(scan_stream_raw(stream1, (const uint8_t *)"abc", 3), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream2, (const uint8_t *)"xxx", 3), MS_FIND_FAIL);

    /* stream1 continues from "abc", so "def" completes the rule. */
    EXPECT_EQ(scan_stream_raw(stream1, (const uint8_t *)"def", 3), MS_SUCCESS);

    /* stream2 is independent and should not inherit stream1's state. */
    EXPECT_EQ(scan_stream_raw(stream2, (const uint8_t *)"def", 3), MS_FIND_FAIL);

    EXPECT_EQ(ms_close_handler_stream(stream1), MS_SUCCESS);
    EXPECT_EQ(ms_close_handler_stream(stream2), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_interleaved_streams(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream1 = NULL;
    ms_stream_t *stream2 = NULL;

    ms_rule_t rules[] = {
        make_rule(2101, "abcdef"),
        make_rule(2102, "12345"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 2), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream1), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream2), MS_SUCCESS);

    EXPECT_EQ(scan_stream_raw(stream1, (const uint8_t *)"abc", 3), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream2, (const uint8_t *)"12", 2), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream1, (const uint8_t *)"def", 3), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream2, (const uint8_t *)"345", 3), MS_SUCCESS);

    EXPECT_EQ(ms_close_handler_stream(stream1), MS_SUCCESS);
    EXPECT_EQ(ms_close_handler_stream(stream2), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_prefix_and_long_rule(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    ms_rule_t rules[] = {
        make_rule(2201, "123"),
        make_rule(2202, "12345"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 2), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"xx123", 5), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"45yy", 4), MS_SUCCESS);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_match_then_continue(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    ms_rule_t rules[] = {
        make_rule(2301, "abc"),
        make_rule(2302, "def"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 2), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"xxabc", 5), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"yydef", 5), MS_SUCCESS);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_reset(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    ms_rule_t rules[] = {
        make_rule(3001, "abcdef"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 1), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);

    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abc", 3), MS_FIND_FAIL);
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);

    /*
     * After reset, the stream no longer remembers "abc",
     * so scanning "def" alone should not match "abcdef".
     */
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"def", 3), MS_FIND_FAIL);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_empty_chunk_keeps_state(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    ms_rule_t rules[] = {
        make_rule(3101, "abcde"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 1), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abc", 3), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"", 0), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"de", 2), MS_SUCCESS);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_stale_after_remove(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    ms_rule_t rules[] = {
        make_rule(4001, "abcde"),
        make_rule(4002, "xyz"),
    };

    ms_rule_t remove_rule[] = {
        make_rule(4001, "abcde"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 2), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abc", 3), MS_FIND_FAIL);

    /*
     * Removing rules rebuilds the AC table and reindexes states.
     * The old stream state must be treated as stale.
     */
    EXPECT_EQ(ms_remove_rule(&handler, remove_rule, 1), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"de", 2), MS_STREAM_STALE);

    /*
     * reset_stream refreshes generation and returns to root state.
     * The removed rule should no longer match.
     */
    EXPECT_EQ(ms_reset_stream(stream), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abcde", 5), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"xyz", 3), MS_SUCCESS);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_remove_fail_keeps_stream_valid(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    ms_rule_t rules[] = {
        make_rule(4101, "abcde"),
    };
    ms_rule_t remove_rule[] = {
        make_rule(4199, "xxxxx"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, rules, 1), MS_SUCCESS);

    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abc", 3), MS_FIND_FAIL);
    EXPECT_EQ(ms_remove_rule(&handler, remove_rule, 1), MS_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"de", 2), MS_SUCCESS);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_stream_invalid_usage(void)
{
    ms_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    ms_rule_t rules[] = {
        make_rule(5001, "abc"),
    };

    EXPECT_EQ(ms_open_handler_stream(NULL, &stream), MS_INVALID_PARA);
    EXPECT_EQ(ms_open_handler_stream(&handler, NULL), MS_INVALID_PARA);
    EXPECT_EQ(ms_reset_stream(NULL), MS_INVALID_PARA);
    EXPECT_EQ(ms_close_handler_stream(NULL), MS_INVALID_PARA);

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);

    /* Cannot open stream before AC rules are built. */
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_INVALID_PARA);

    EXPECT_EQ(ms_add_rule(&handler, rules, 1), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_SUCCESS);
    EXPECT_EQ(ms_open_handler_stream(&handler, &stream), MS_INVALID_PARA);

    EXPECT_EQ(scan_stream_raw(NULL, (const uint8_t *)"abc", 3), MS_INVALID_PARA);
    EXPECT_EQ(scan_stream_raw(stream, NULL, 3), MS_INVALID_PARA);

    EXPECT_EQ(ms_close_handler_stream(stream), MS_SUCCESS);
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_cross_chunk_match);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_hscollider_style_splits);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_multi_chunk_match);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_independent_streams);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_interleaved_streams);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_prefix_and_long_rule);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_match_then_continue);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_reset);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_empty_chunk_keeps_state);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_stale_after_remove);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_remove_fail_keeps_stream_valid);
KTEST_ADD_TEST_CASE(SuiteRuleStreamCompat, test_ac_stream_invalid_usage);


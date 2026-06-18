/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: AC stream unit tests
 */
#include <string.h>
#include <stdint.h>
#include "ktest.h"
#include "../../../mmpm/include/mmpm.h"
#include "../../../mmpm/include/mmpm_handler.h"
#include "../../../mmpm/include/mmpm_compile.h"
#include "../../../mmpm/include/mmpm_rule.h"
#include "../../../mmpm/include/mmpm_runtime.h"

static mmpm_rule_t make_rule(uint32_t alias, const char *s)
{
    mmpm_rule_t rule;

    rule.alias = alias;
    rule.key = (uint8_t *)s;
    rule.key_len = (uint32_t)strlen(s);

    return rule;
}

static int scan_stream_raw(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    return mmpm_scan_rule_stream(stream, data, len);
}

static int scan_stream_by_blocks(mmpm_handler_t *handler, const char *data, uint32_t stream_blocks)
{
    ms_stream_t *stream = NULL;
    uint32_t len = (uint32_t)strlen(data);
    uint32_t block_size;
    uint32_t pos = 0;
    int final_ret = MMPM_FIND_FAIL;
    int ret;

    if (stream_blocks == 0) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_open_handler_stream(handler, &stream);
    EXPECT_EQ(ret, MMPM_SUCCESS);
    if (ret != MMPM_SUCCESS) {
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
        if (ret == MMPM_SUCCESS) {
            final_ret = MMPM_SUCCESS;
        } else if (ret != MMPM_FIND_FAIL) {
            final_ret = ret;
            break;
        }

        pos += cur;
    }

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    return final_ret;
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
    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"xx123", 5), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"45yy", 4), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_hscollider_style_splits(void)
{
    mmpm_handler_t handler = {0};
    mmpm_rule_t rules[] = {
        make_rule(1101, "12345"),
    };
    mmpm_rule_t full_query = make_rule(0, "xx12345yy");

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_find_rule(&handler, &full_query), MMPM_SUCCESS);

    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 1), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 2), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 3), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 5), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_by_blocks(&handler, "xx12345yy", 20), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_multi_chunk_match(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    mmpm_rule_t rules[] = {
        make_rule(1201, "abcdef"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"ab", 2), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"cd", 2), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"ef", 2), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
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

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream2), MMPM_SUCCESS);

    EXPECT_EQ(scan_stream_raw(stream1, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream2, (const uint8_t *)"xxx", 3), MMPM_FIND_FAIL);

    /* stream1 continues from "abc", so "def" completes the rule. */
    EXPECT_EQ(scan_stream_raw(stream1, (const uint8_t *)"def", 3), MMPM_SUCCESS);

    /* stream2 is independent and should not inherit stream1's state. */
    EXPECT_EQ(scan_stream_raw(stream2, (const uint8_t *)"def", 3), MMPM_FIND_FAIL);

    EXPECT_EQ(mmpm_close_handler_stream(stream1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_close_handler_stream(stream2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_interleaved_streams(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream1 = NULL;
    ms_stream_t *stream2 = NULL;

    mmpm_rule_t rules[] = {
        make_rule(2101, "abcdef"),
        make_rule(2102, "12345"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 2), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream2), MMPM_SUCCESS);

    EXPECT_EQ(scan_stream_raw(stream1, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream2, (const uint8_t *)"12", 2), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream1, (const uint8_t *)"def", 3), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream2, (const uint8_t *)"345", 3), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_handler_stream(stream1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_close_handler_stream(stream2), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_prefix_and_long_rule(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(2201, "123"),
        make_rule(2202, "12345"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 2), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"xx123", 5), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"45yy", 4), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_match_then_continue(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(2301, "abc"),
        make_rule(2302, "def"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 2), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"xxabc", 5), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"yydef", 5), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
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

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);

    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);

    /*
     * After reset, the stream no longer remembers "abc",
     * so scanning "def" alone should not match "abcdef".
     */
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"def", 3), MMPM_FIND_FAIL);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_empty_chunk_keeps_state(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(3101, "abcde"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"", 0), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"de", 2), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
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

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);

    /*
     * Removing rules rebuilds the AC table and reindexes states.
     * The old stream state must be treated as stale.
     */
    EXPECT_EQ(mmpm_remove_rule(&handler, remove_rule, 1), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"de", 2), MMPM_STREAM_STALE);

    /*
     * reset_stream refreshes generation and returns to root state.
     * The removed rule should no longer match.
     */
    EXPECT_EQ(mmpm_reset_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abcde", 5), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"xyz", 3), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_remove_fail_keeps_stream_valid(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(4101, "abcde"),
    };
    mmpm_rule_t remove_rule[] = {
        make_rule(4199, "xxxxx"),
    };

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    EXPECT_EQ(mmpm_remove_rule(&handler, remove_rule, 1), MMPM_FIND_FAIL);
    EXPECT_EQ(scan_stream_raw(stream, (const uint8_t *)"de", 2), MMPM_SUCCESS);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

static void test_ac_stream_invalid_usage(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(5001, "abc"),
    };

    EXPECT_EQ(mmpm_open_handler_stream(NULL, &stream), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_open_handler_stream(&handler, NULL), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_reset_stream(NULL), MMPM_INVALID_PARA);
    EXPECT_EQ(mmpm_close_handler_stream(NULL), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);

    /* Cannot open stream before AC rules are built. */
    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_open_handler_stream(&handler, &stream), MMPM_INVALID_PARA);

    EXPECT_EQ(scan_stream_raw(NULL, (const uint8_t *)"abc", 3), MMPM_INVALID_PARA);
    EXPECT_EQ(scan_stream_raw(stream, NULL, 3), MMPM_INVALID_PARA);

    EXPECT_EQ(mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    EXPECT_EQ(mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_cross_chunk_match);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_hscollider_style_splits);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_multi_chunk_match);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_independent_streams);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_interleaved_streams);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_prefix_and_long_rule);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_match_then_continue);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_reset);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_empty_chunk_keeps_state);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_stale_after_remove);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_remove_fail_keeps_stream_valid);
KTEST_ADD_TEST_CASE(SuiteAcStream, test_ac_stream_invalid_usage);


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mmpm.h"

/*
 * 本 demo 覆盖 AC stream 的主要使用场景：
 * 1. 对比 block 模式和 stream 模式的跨 chunk 匹配能力。
 * 2. 模拟 hscollider -t，把同一段语料按不同块数切分后扫描。
 * 3. 验证规则跨三个及以上 chunk 时仍能匹配。
 * 4. 验证多条 stream 交错扫描时状态互不影响。
 * 5. 验证短规则和长规则共存，例如 "123" 和 "12345"。
 * 6. 验证一次命中后，同一条 stream 还能继续扫描并再次命中。
 * 7. 验证空 chunk 不破坏状态，以及 reset 后状态被清空。
 * 8. 验证 remove 规则后旧 stream 变 stale，reset 后可继续使用。
 * 9. 验证 remove 失败时 stream 状态仍然有效。
 * 10. 验证 stream API 的非法参数返回。
 */

/*
 *运行方式：
 *1. cd至Megascan根目录，如：cd /home/Megascan；
 *2. 编译Megascan: rm -rf build output && cmake -S . -B build && cmake --build build -j$(nproc);
 *3. 在Megascan根目录执行：
    gcc examples/stream_demo.c \
    -I./output/include \
    -L./output/lib \
    -lmmpm \
    -Wl,-rpath,$PWD/output/lib \
    -o examples/stream_demo
 *4. 运行代码：./examples/stream_demo
 *5. ALL STREAM DEMO TESTS PASSED打印则全部可用
 */

static int g_failures = 0;

/* 构造一条 mmpm 规则或查询对象，避免重复填写 alias/key_len/key。 */
static mmpm_rule_t make_rule(uint32_t alias, const char *s)
{
    mmpm_rule_t rule;

    rule.alias = alias;
    rule.key_len = (uint32_t)strlen(s);
    rule.key = (uint8_t *)s;

    return rule;
}

/* 校验接口返回值是否符合预期，并打印 PASS/FAIL 结果。 */
static void expect_ret(const char *name, int got, int expected)
{
    if (got == expected) {
        printf("[PASS] %s: %d\n", name, got);
        return;
    }

    printf("[FAIL] %s: got %d, expected %d\n", name, got, expected);
    g_failures++;
}

static int scan_stream_raw(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    return mmpm_scan_rule_stream(stream, data, len);
}

/*
 * 按指定块数切分同一段语料并使用 stream 扫描，
 * 用于模拟 hyperscan hscollider -t 风格的多切分测试。
 */
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

    ret = mmpm_close_handler_stream(stream);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    return final_ret;
}

/* 验证 block 模式无法跨 buffer 匹配，而 stream 模式可以跨 chunk 匹配。 */
static void test_cross_chunk_match(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    int ret;

    mmpm_rule_t rules[] = {
        make_rule(1001, "12345"),
        make_rule(1002, "xyz"),
    };
    mmpm_rule_t block_query1 = make_rule(0, "xx123");
    mmpm_rule_t block_query2 = make_rule(0, "45yy");

    printf("\n== cross chunk match ==\n");

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("add rules", mmpm_add_rule(&handler, rules, 2), MMPM_SUCCESS);

    expect_ret("block xx123", mmpm_find_rule(&handler, &block_query1), MMPM_FIND_FAIL);
    expect_ret("block 45yy", mmpm_find_rule(&handler, &block_query2), MMPM_FIND_FAIL);

    expect_ret("open stream", mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    expect_ret("scan xx123", scan_stream_raw(stream, (const uint8_t *)"xx123", 5), MMPM_FIND_FAIL);
    expect_ret("scan 45yy", scan_stream_raw(stream, (const uint8_t *)"45yy", 4), MMPM_SUCCESS);

    ret = mmpm_close_handler_stream(stream);
    expect_ret("close stream", ret, MMPM_SUCCESS);
    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 验证同一段语料按不同块数切分后，stream 模式仍能得到匹配结果。 */
static void test_hscollider_style_splits(void)
{
    mmpm_handler_t handler = {0};
    mmpm_rule_t rules[] = {
        make_rule(1101, "12345"),
    };
    mmpm_rule_t full_query = make_rule(0, "xx12345yy");

    printf("\n== hscollider style splits ==\n");

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("add rules", mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);
    expect_ret("full block", mmpm_find_rule(&handler, &full_query), MMPM_SUCCESS);

    expect_ret("split 1", scan_stream_by_blocks(&handler, "xx12345yy", 1), MMPM_SUCCESS);
    expect_ret("split 2", scan_stream_by_blocks(&handler, "xx12345yy", 2), MMPM_SUCCESS);
    expect_ret("split 3", scan_stream_by_blocks(&handler, "xx12345yy", 3), MMPM_SUCCESS);
    expect_ret("split 5", scan_stream_by_blocks(&handler, "xx12345yy", 5), MMPM_SUCCESS);
    expect_ret("split 20", scan_stream_by_blocks(&handler, "xx12345yy", 20), MMPM_SUCCESS);

    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 验证一个规则跨三个 chunk 时，stream 状态能连续传递直到最终命中。 */
static void test_multi_chunk_match(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    mmpm_rule_t rules[] = {
        make_rule(1201, "abcdef"),
    };

    printf("\n== multi chunk match ==\n");

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("add rules", mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);
    expect_ret("open stream", mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);

    expect_ret("scan ab", scan_stream_raw(stream, (const uint8_t *)"ab", 2), MMPM_FIND_FAIL);
    expect_ret("scan cd", scan_stream_raw(stream, (const uint8_t *)"cd", 2), MMPM_FIND_FAIL);
    expect_ret("scan ef", scan_stream_raw(stream, (const uint8_t *)"ef", 2), MMPM_SUCCESS);

    expect_ret("close stream", mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 验证两条 stream 交错扫描时，各自维护独立的 AC 状态。 */
static void test_interleaved_streams(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream1 = NULL;
    ms_stream_t *stream2 = NULL;

    mmpm_rule_t rules[] = {
        make_rule(2001, "abcdef"),
        make_rule(2002, "12345"),
    };

    printf("\n== interleaved streams ==\n");

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("add rules", mmpm_add_rule(&handler, rules, 2), MMPM_SUCCESS);
    expect_ret("open stream1", mmpm_open_handler_stream(&handler, &stream1), MMPM_SUCCESS);
    expect_ret("open stream2", mmpm_open_handler_stream(&handler, &stream2), MMPM_SUCCESS);

    expect_ret("stream1 abc", scan_stream_raw(stream1, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    expect_ret("stream2 12", scan_stream_raw(stream2, (const uint8_t *)"12", 2), MMPM_FIND_FAIL);
    expect_ret("stream1 def", scan_stream_raw(stream1, (const uint8_t *)"def", 3), MMPM_SUCCESS);
    expect_ret("stream2 345", scan_stream_raw(stream2, (const uint8_t *)"345", 3), MMPM_SUCCESS);

    expect_ret("close stream1", mmpm_close_handler_stream(stream1), MMPM_SUCCESS);
    expect_ret("close stream2", mmpm_close_handler_stream(stream2), MMPM_SUCCESS);
    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 验证短规则和长规则共享前缀时，短规则命中后长规则仍可继续跨 chunk 命中。 */
static void test_prefix_and_continue(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(3001, "123"),
        make_rule(3002, "12345"),
        make_rule(3003, "abc"),
        make_rule(3004, "def"),
    };

    printf("\n== prefix and continue ==\n");

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("add rules", mmpm_add_rule(&handler, rules, 4), MMPM_SUCCESS);
    expect_ret("open stream", mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);

    expect_ret("short prefix hit", scan_stream_raw(stream, (const uint8_t *)"xx123", 5), MMPM_SUCCESS);
    expect_ret("long prefix hit", scan_stream_raw(stream, (const uint8_t *)"45yy", 4), MMPM_SUCCESS);
    expect_ret("later hit abc", scan_stream_raw(stream, (const uint8_t *)"abc", 3), MMPM_SUCCESS);
    expect_ret("later hit def", scan_stream_raw(stream, (const uint8_t *)"def", 3), MMPM_SUCCESS);

    expect_ret("close stream", mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 验证空 chunk 不改变 stream 状态，同时验证 reset 会清空已有的中间状态。 */
static void test_reset_and_empty_chunk(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(4001, "abcdef"),
        make_rule(4002, "abcde"),
    };

    printf("\n== reset and empty chunk ==\n");

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("add rules", mmpm_add_rule(&handler, rules, 2), MMPM_SUCCESS);
    expect_ret("open stream", mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);

    expect_ret("scan abc", scan_stream_raw(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    expect_ret("empty chunk", scan_stream_raw(stream, (const uint8_t *)"", 0), MMPM_FIND_FAIL);
    expect_ret("scan de after empty", scan_stream_raw(stream, (const uint8_t *)"de", 2), MMPM_SUCCESS);

    expect_ret("reset", mmpm_reset_stream(stream), MMPM_SUCCESS);
    expect_ret("scan def after reset", scan_stream_raw(stream, (const uint8_t *)"def", 3), MMPM_FIND_FAIL);

    expect_ret("close stream", mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 验证 remove 成功后自动机版本变化，旧 stream 会返回 stale，reset 后可继续使用。 */
static void test_remove_and_stale(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(5001, "abcde"),
        make_rule(5002, "xyz"),
    };
    mmpm_rule_t remove_abcde[] = {
        make_rule(5001, "abcde"),
    };

    printf("\n== remove and stale ==\n");

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("add rules", mmpm_add_rule(&handler, rules, 2), MMPM_SUCCESS);
    expect_ret("open stream", mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);

    expect_ret("scan abc", scan_stream_raw(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    expect_ret("remove abcde", mmpm_remove_rule(&handler, remove_abcde, 1), MMPM_SUCCESS);
    expect_ret("scan stale stream", scan_stream_raw(stream, (const uint8_t *)"de", 2), MMPM_STREAM_STALE);

    expect_ret("reset after stale", mmpm_reset_stream(stream), MMPM_SUCCESS);
    expect_ret("removed rule no match", scan_stream_raw(stream, (const uint8_t *)"abcde", 5), MMPM_FIND_FAIL);
    expect_ret("remaining rule match", scan_stream_raw(stream, (const uint8_t *)"xyz", 3), MMPM_SUCCESS);

    expect_ret("close stream", mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 验证 remove 不存在的规则失败时，不会破坏已有 stream 的连续匹配状态。 */
static void test_remove_fail_keeps_stream_valid(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;

    mmpm_rule_t rules[] = {
        make_rule(6001, "abcde"),
    };
    mmpm_rule_t remove_missing[] = {
        make_rule(6999, "xxxxx"),
    };

    printf("\n== remove fail keeps stream valid ==\n");

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("add rules", mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);
    expect_ret("open stream", mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);

    expect_ret("scan abc", scan_stream_raw(stream, (const uint8_t *)"abc", 3), MMPM_FIND_FAIL);
    expect_ret("remove missing", mmpm_remove_rule(&handler, remove_missing, 1), MMPM_FIND_FAIL);
    expect_ret("scan de still valid", scan_stream_raw(stream, (const uint8_t *)"de", 2), MMPM_SUCCESS);

    expect_ret("close stream", mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 验证 stream API 在空指针、未 build、重复 open 等非法用法下返回错误码。 */
static void test_invalid_usage(void)
{
    mmpm_handler_t handler = {0};
    ms_stream_t *stream = NULL;
    mmpm_rule_t rules[] = {
        make_rule(7001, "abc"),
    };

    printf("\n== invalid usage ==\n");

    expect_ret("open NULL handler", mmpm_open_handler_stream(NULL, &stream), MMPM_INVALID_PARA);
    expect_ret("open NULL stream pointer", mmpm_open_handler_stream(&handler, NULL), MMPM_INVALID_PARA);
    expect_ret("reset NULL", mmpm_reset_stream(NULL), MMPM_INVALID_PARA);
    expect_ret("close NULL", mmpm_close_handler_stream(NULL), MMPM_INVALID_PARA);

    expect_ret("init", mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_AC, 0, 0), MMPM_SUCCESS);
    expect_ret("open before build", mmpm_open_handler_stream(&handler, &stream), MMPM_INVALID_PARA);

    expect_ret("add rules", mmpm_add_rule(&handler, rules, 1), MMPM_SUCCESS);
    expect_ret("open stream", mmpm_open_handler_stream(&handler, &stream), MMPM_SUCCESS);
    expect_ret("open into non-null stream", mmpm_open_handler_stream(&handler, &stream), MMPM_INVALID_PARA);
    expect_ret("scan NULL stream", scan_stream_raw(NULL, (const uint8_t *)"abc", 3), MMPM_INVALID_PARA);
    expect_ret("scan NULL data", scan_stream_raw(stream, NULL, 3), MMPM_INVALID_PARA);

    expect_ret("close stream", mmpm_close_handler_stream(stream), MMPM_SUCCESS);
    expect_ret("destroy", mmpm_handler_destroy(&handler), MMPM_SUCCESS);
}

/* 依次运行所有 demo 测试，并根据失败数量返回进程退出码。 */
int main(void)
{
    test_cross_chunk_match();
    test_hscollider_style_splits();
    test_multi_chunk_match();
    test_interleaved_streams();
    test_prefix_and_continue();
    test_reset_and_empty_chunk();
    test_remove_and_stale();
    test_remove_fail_keeps_stream_valid();
    test_invalid_usage();

    if (g_failures == 0) {
        printf("\nALL STREAM DEMO TESTS PASSED\n");
        return 0;
    }

    printf("\nSTREAM DEMO TESTS FAILED: %d\n", g_failures);
    return 1;
}

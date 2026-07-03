/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: legacy AC rule remove compatibility unit tests
 */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "ktest.h"
#include "../../../ms/include/ms.h"
#include "../../../ms/include/ms_ac.h"
#include "../../../ms/include/ms_allocator.h"
#include "../../../ms/include/ms_handler.h"
#include "../../../ms/include/ms_compile.h"
#include "../../../ms/include/ms_rule.h"

static ms_rule_t make_rule(uint32_t alias, const char *s)
{
    ms_rule_t rule;
    rule.alias = alias;
    rule.key = (uint8_t *)s;
    rule.key_len = (uint32_t)strlen(s);
    return rule;
}

typedef struct {
    uint32_t malloc_count;
    uint32_t fail_on_malloc;
} failing_alloc_state_t;

static failing_alloc_state_t g_failing_alloc_state;

static void reset_failing_alloc_state(void)
{
    memset(&g_failing_alloc_state, 0, sizeof(g_failing_alloc_state));
}

static void *failing_db_alloc(size_t size)
{
    g_failing_alloc_state.malloc_count++;
    if (g_failing_alloc_state.fail_on_malloc != 0 &&
        g_failing_alloc_state.malloc_count == g_failing_alloc_state.fail_on_malloc) {
        return NULL;
    }
    return malloc(size);
}

static void *failing_db_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

static void failing_db_free(void *ptr)
{
    free(ptr);
}

static void reset_allocators(void)
{
    (void)ms_set_allocator(NULL, NULL, NULL);
}

static void expect_match(ms_handler_t *handler, const char *data, int expected)
{
    ms_rule_t query = {0};
    query.key = (uint8_t *)data;
    query.key_len = (uint32_t)strlen(data);
    EXPECT_EQ(ms_find_rule(handler, &query), expected);
}

static void test_ac_remove_basic_exact()
{
    ms_handler_t handler = {0};
    ms_rule_t add_rules[] = {
        make_rule(1001, "alpha"),
        make_rule(1002, "beta"),
        make_rule(1003, "gamma"),
    };
    ms_rule_t remove_beta[] = { make_rule(1002, "beta") };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    expect_match(&handler, "xxbetaxx", MS_SUCCESS);
    EXPECT_EQ(ms_remove_rule(&handler, remove_beta, 1), MS_SUCCESS);
    expect_match(&handler, "xxbetaxx", MS_FIND_FAIL);
    expect_match(&handler, "00alpha11", MS_SUCCESS);
    expect_match(&handler, "00gamma11", MS_SUCCESS);
    EXPECT_EQ(ms_remove_rule(&handler, remove_beta, 1), MS_FIND_FAIL);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_remove_shared_prefix_prune()
{
    ms_handler_t handler = {0};
    ms_rule_t add_rules[] = {
        make_rule(2001, "abcd"),
        make_rule(2002, "abce"),
        make_rule(2003, "abcf"),
        make_rule(2004, "abx"),
        make_rule(2005, "zabcfq"),
    };
    ms_rule_t remove_rules[] = {
        make_rule(2002, "abce"),
        make_rule(2001, "abcd"),
        make_rule(2003, "abcf"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    expect_match(&handler, "11abce22", MS_SUCCESS);
    EXPECT_EQ(ms_remove_rule(&handler, &remove_rules[0], 1), MS_SUCCESS);
    expect_match(&handler, "11abce22", MS_FIND_FAIL);
    expect_match(&handler, "11abcd22", MS_SUCCESS);
    expect_match(&handler, "11abcf22", MS_SUCCESS);
    expect_match(&handler, "11abx22", MS_SUCCESS);
    expect_match(&handler, "11zabcfq22", MS_SUCCESS);

    EXPECT_EQ(ms_remove_rule(&handler, &remove_rules[1], 1), MS_SUCCESS);
    EXPECT_EQ(ms_remove_rule(&handler, &remove_rules[2], 1), MS_SUCCESS);
    expect_match(&handler, "11abcd22", MS_FIND_FAIL);
    expect_match(&handler, "11abcf22", MS_FIND_FAIL);
    expect_match(&handler, "11abx22", MS_SUCCESS);
    expect_match(&handler, "11zabcfq22", MS_SUCCESS);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_remove_overlap_and_fail_rebuild()
{
    ms_handler_t handler = {0};
    ms_rule_t add_rules[] = {
        make_rule(3001, "his"),
        make_rule(3002, "she"),
        make_rule(3003, "hers"),
        make_rule(3004, "hero"),
        make_rule(3005, "shore"),
        make_rule(3006, "short"),
        make_rule(3007, "or"),
    };
    ms_rule_t remove_his[] = { make_rule(3001, "his") };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    expect_match(&handler, "xxhisyy", MS_SUCCESS);
    expect_match(&handler, "ushers", MS_SUCCESS);
    expect_match(&handler, "myhero", MS_SUCCESS);
    expect_match(&handler, "shorter", MS_SUCCESS);

    EXPECT_EQ(ms_remove_rule(&handler, remove_his, 1), MS_SUCCESS);
    expect_match(&handler, "xxhisyy", MS_FIND_FAIL);
    expect_match(&handler, "ushers", MS_SUCCESS);
    expect_match(&handler, "myhero", MS_SUCCESS);
    expect_match(&handler, "shorter", MS_SUCCESS);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_remove_same_pattern_multi_alias()
{
    ms_handler_t handler = {0};
    ms_rule_t add_rules[] = {
        make_rule(4001, "token"),
        make_rule(4002, "token"),
        make_rule(4003, "tokyo"),
    };
    ms_rule_t remove_rules[] = {
        make_rule(4001, "token"),
        make_rule(4002, "token"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    expect_match(&handler, "aatokenbb", MS_SUCCESS);
    EXPECT_EQ(ms_remove_rule(&handler, &remove_rules[0], 1), MS_SUCCESS);
    expect_match(&handler, "aatokenbb", MS_SUCCESS);
    EXPECT_EQ(ms_remove_rule(&handler, &remove_rules[1], 1), MS_SUCCESS);
    expect_match(&handler, "aatokenbb", MS_FIND_FAIL);
    expect_match(&handler, "xxtokyoyy", MS_SUCCESS);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_remove_to_empty_and_boundaries()
{
    ms_handler_t handler = {0};
    ms_rule_t add_rules[] = {
        make_rule(5001, "m1"),
        make_rule(5002, "m2"),
    };
    ms_rule_t remove_m1[] = { make_rule(5001, "m1") };
    ms_rule_t remove_m2[] = { make_rule(5002, "m2") };
    ms_rule_t invalid_len_rule = {5003, 0, (uint8_t *)"x"};

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    EXPECT_EQ(ms_remove_rule(&handler, remove_m1, 1), MS_SUCCESS);
    EXPECT_EQ(ms_remove_rule(&handler, remove_m2, 1), MS_SUCCESS);
    expect_match(&handler, "xxm1yy", MS_FIND_FAIL);
    expect_match(&handler, "xxm2yy", MS_FIND_FAIL);
    expect_match(&handler, "nomatch", MS_FIND_FAIL);

    EXPECT_EQ(ms_remove_rule(&handler, remove_m1, 1), MS_FIND_FAIL);
    EXPECT_EQ(ms_remove_rule(&handler, &invalid_len_rule, 1), MS_INVALID_PARA);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_remove_complex_rule_set_regression()
{
    ms_handler_t handler = {0};
    ms_rule_t add_rules[] = {
        make_rule(6001, "netflow"),
        make_rule(6002, "netflux"),
        make_rule(6003, "network"),
        make_rule(6004, "workflow"),
        make_rule(6005, "workshop"),
        make_rule(6006, "alpha-beta"),
        make_rule(6007, "alpha_gamma"),
        make_rule(6008, "proto:v1"),
        make_rule(6009, "proto:v2"),
        make_rule(6010, "xyz123"),
        make_rule(6011, "123xyz"),
        make_rule(6012, "zz-top"),
    };
    ms_rule_t remove_rules[] = {
        make_rule(6002, "netflux"),
        make_rule(6004, "workflow"),
        make_rule(6008, "proto:v1"),
        make_rule(6010, "xyz123"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    expect_match(&handler, "xxnetflowyy", MS_SUCCESS);
    expect_match(&handler, "xxnetfluxyy", MS_SUCCESS);
    expect_match(&handler, "zzworkflowzz", MS_SUCCESS);
    expect_match(&handler, "zzproto:v1zz", MS_SUCCESS);
    expect_match(&handler, "zzproto:v2zz", MS_SUCCESS);
    expect_match(&handler, "zzxyz123zz", MS_SUCCESS);
    expect_match(&handler, "zz123xyzzz", MS_SUCCESS);

    EXPECT_EQ(ms_remove_rule(&handler, remove_rules, (uint32_t)(sizeof(remove_rules) / sizeof(remove_rules[0]))), MS_SUCCESS);

    expect_match(&handler, "xxnetfluxyy", MS_FIND_FAIL);
    expect_match(&handler, "zzworkflowzz", MS_FIND_FAIL);
    expect_match(&handler, "zzproto:v1zz", MS_FIND_FAIL);
    expect_match(&handler, "zzxyz123zz", MS_FIND_FAIL);

    expect_match(&handler, "xxnetflowyy", MS_SUCCESS);
    expect_match(&handler, "xxnetworkyy", MS_SUCCESS);
    expect_match(&handler, "zzworkshopzz", MS_SUCCESS);
    expect_match(&handler, "zzproto:v2zz", MS_SUCCESS);
    expect_match(&handler, "zz123xyzzz", MS_SUCCESS);
    expect_match(&handler, "xxzz-topyy", MS_SUCCESS);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_remove_deleted_rule_as_multi_fail_target()
{
    ms_handler_t handler = {0};
    ms_rule_t add_rules[] = {
        make_rule(7001, "bc"),      
        make_rule(7002, "abc"),
        make_rule(7003, "xbc"),
        make_rule(7004, "zbc"),
        make_rule(7005, "xxbcx"),
    };
    ms_rule_t remove_bc[] = {
        make_rule(7001, "bc"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    expect_match(&handler, "00bc11", MS_SUCCESS);
    expect_match(&handler, "00abc11", MS_SUCCESS);
    expect_match(&handler, "00xbc11", MS_SUCCESS);
    expect_match(&handler, "00zbc11", MS_SUCCESS);
    expect_match(&handler, "00xxbcx11", MS_SUCCESS);

    EXPECT_EQ(ms_remove_rule(&handler, remove_bc, 1), MS_SUCCESS);

    expect_match(&handler, "00bc11", MS_FIND_FAIL);
    expect_match(&handler, "qqqbcqqq", MS_FIND_FAIL);

    expect_match(&handler, "00abc11", MS_SUCCESS);
    expect_match(&handler, "00xbc11", MS_SUCCESS);
    expect_match(&handler, "00zbc11", MS_SUCCESS);
    expect_match(&handler, "00xxbcx11", MS_SUCCESS);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_remove_nonexistent_rule()
{
    ms_handler_t handler = {0};
    ms_rule_t add_rules[] = {
        make_rule(8001, "apple"),
        make_rule(8002, "banana"),
    };
    ms_rule_t remove_not_found_key[] = {
        make_rule(8999, "orange"),
    };
    ms_rule_t remove_not_found_alias[] = {
        make_rule(8998, "apple"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    expect_match(&handler, "xxappleyy", MS_SUCCESS);
    expect_match(&handler, "xxbananayy", MS_SUCCESS);

    EXPECT_EQ(ms_remove_rule(&handler, remove_not_found_key, 1), MS_FIND_FAIL);
    expect_match(&handler, "xxappleyy", MS_SUCCESS);
    expect_match(&handler, "xxbananayy", MS_SUCCESS);

    EXPECT_EQ(ms_remove_rule(&handler, remove_not_found_alias, 1), MS_FIND_FAIL);
    expect_match(&handler, "xxappleyy", MS_SUCCESS);
    expect_match(&handler, "xxbananayy", MS_SUCCESS);

    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

static void test_ac_remove_rebuild_alloc_failure_cleans_partial_table(void)
{
    ms_handler_t handler = {0};
    ACAutomaton *automaton;
    ms_rule_t add_rules[] = {
        make_rule(9001, "a"),
        make_rule(9002, "b"),
    };
    ms_rule_t remove_a[] = {
        make_rule(9001, "a"),
    };

    EXPECT_EQ(ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0), MS_SUCCESS);
    EXPECT_EQ(ms_add_rule(&handler, add_rules, (uint32_t)(sizeof(add_rules) / sizeof(add_rules[0]))), MS_SUCCESS);

    reset_failing_alloc_state();
    g_failing_alloc_state.fail_on_malloc = 3;
    EXPECT_EQ(ms_set_database_allocator(failing_db_alloc, failing_db_calloc, failing_db_free), MS_SUCCESS);

    EXPECT_EQ(ms_remove_rule(&handler, remove_a, 1), MS_MALLOC_FAIL);

    automaton = (ACAutomaton *)handler.ms_matcher;
    EXPECT_TRUE(automaton != NULL);
    EXPECT_TRUE(automaton->table == NULL);
    EXPECT_TRUE(automaton->output == NULL);
    EXPECT_TRUE(automaton->output_size == NULL);

    reset_allocators();
    EXPECT_EQ(ms_handler_destroy(&handler), MS_SUCCESS);
}

KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_basic_exact);
KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_shared_prefix_prune);
KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_overlap_and_fail_rebuild);
KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_same_pattern_multi_alias);
KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_to_empty_and_boundaries);
KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_complex_rule_set_regression);
KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_deleted_rule_as_multi_fail_target);
KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_nonexistent_rule);
KTEST_ADD_TEST_CASE(SuiteRuleRemoveCompat, test_ac_remove_rebuild_alloc_failure_cleans_partial_table);

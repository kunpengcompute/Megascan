/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "demo_common.h"

#include <stdio.h>
#include <string.h>

#include "ms_handler.h"
#include "ms_rule.h"

static const char *const g_legacy_blacklist_apis[] = {
    "ms_handler_init",
    "ms_handler_destroy",
    "ms_set_hash_func",
    "ms_add_rule",
    "ms_find_rule",
    "ms_remove_rule",
    NULL
};

static const char *const g_legacy_content_scan_apis[] = {
    "ms_handler_init",
    "ms_handler_destroy",
    "ms_add_rule",
    "ms_find_rule",
    "ms_remove_rule",
    NULL
};

const demo_scenario_t g_demo_scenario_legacy_blacklist = {
    "legacy-blacklist",
    "旧版黑名单查询",
    "通过旧版 HASH handler 加载并精确查询黑名单。",
    g_legacy_blacklist_apis,
    demo_legacy_hash_handler_apis
};

const demo_scenario_t g_demo_scenario_legacy_content_scan = {
    "legacy-content-scan",
    "旧版内容扫描",
    "通过旧版 AC handler 加载文本规则并扫描内容。",
    g_legacy_content_scan_apis,
    demo_legacy_ac_rule_apis
};

static uint32_t demo_hash(const uint8_t *key, uint32_t key_len)
{
    uint32_t hash = 2166136261u;
    uint32_t i;

    for (i = 0; i < key_len; i++) {
        hash ^= key[i];
        hash *= 16777619u;
    }
    return hash;
}

void demo_legacy_hash_handler_apis(void)
{
    const demo_options_t *options = demo_get_options();
    const char *lookup_key = options->key != NULL ? options->key : "malware-sha256-demo";
    ms_handler_t handler = {0};
    ms_rule_t rules[] = {
        demo_make_rule(1001, "blocked-user"),
        demo_make_rule(1002, lookup_key)
    };
    ms_rule_t query = demo_make_rule(0, lookup_key);
    ms_rule_t remove_rule = demo_make_rule(1002, lookup_key);
    int ret;

    demo_section("旧版黑名单查询");

    ret = demo_step_ret("创建 HASH handler", ms_handler_init(&handler, MS_MATCHER_TYPE_HASH, 16, 4));
    if (ret != MS_SUCCESS) {
        return;
    }
    (void)demo_step_ret("设置自定义 HASH 函数", ms_set_hash_func(&handler, demo_hash));
    (void)demo_step_ret("加载黑名单规则", ms_add_rule(&handler, rules, (uint32_t)DEMO_ARRAY_SIZE(rules)));
    (void)demo_step_ret("查询 IOC key", ms_find_rule(&handler, &query));
    printf("%-48s 命中alias=%u\n", "ms_find_rule(HASH) 输出", query.alias);
    (void)demo_step_ret("删除 IOC key", ms_remove_rule(&handler, &remove_rule, 1));
    query.alias = 0;
    (void)demo_step_ret("再次查询已删除的 IOC key", ms_find_rule(&handler, &query));
    (void)demo_step_ret("销毁 HASH handler", ms_handler_destroy(&handler));
}

void demo_legacy_ac_rule_apis(void)
{
    const demo_options_t *options = demo_get_options();
    const char *primary_pattern = options->pattern != NULL ? options->pattern : "password=";
    const uint8_t *input = options->input != NULL ? options->input :
                           (const uint8_t *)"POST /upload password=secret-value&token=session-token";
    uint32_t input_len = options->input != NULL ? options->input_len :
                         (uint32_t)strlen((const char *)input);
    ms_handler_t handler = {0};
    ms_rule_t rules[] = {
        demo_make_rule(1101, primary_pattern),
        demo_make_rule(1102, "token=")
    };
    ms_rule_t query = {0, input_len, (uint8_t *)input};
    ms_rule_t remove_rule = demo_make_rule(1102, "token=");
    int ret;

    demo_section("旧版内容扫描");

    ret = demo_step_ret("创建 AC handler", ms_handler_init(&handler, MS_MATCHER_TYPE_AC, 0, 0));
    if (ret != MS_SUCCESS) {
        return;
    }
    (void)demo_step_ret("加载敏感内容规则", ms_add_rule(&handler, rules, (uint32_t)DEMO_ARRAY_SIZE(rules)));
    (void)demo_step_ret("扫描请求内容", ms_find_rule(&handler, &query));
    printf("%-48s 命中alias=%u\n", "ms_find_rule(AC) 输出", query.alias);
    (void)demo_step_ret("删除 token 规则", ms_remove_rule(&handler, &remove_rule, 1));
    query.alias = 0;
    (void)demo_step_ret("再次扫描请求内容", ms_find_rule(&handler, &query));
    (void)demo_step_ret("销毁 AC handler", ms_handler_destroy(&handler));
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "demo_common.h"

#include <stdio.h>
#include <string.h>

static demo_options_t g_demo_options;

const demo_options_t *demo_get_options(void)
{
    return &g_demo_options;
}

void demo_set_options(const demo_options_t *options)
{
    if (options == NULL) {
        (void)memset(&g_demo_options, 0, sizeof(g_demo_options));
        return;
    }
    g_demo_options = *options;
}

static const char *demo_ret_name(int ret)
{
    switch (ret) {
        case MS_SUCCESS:
            return "MS_SUCCESS";
        case MS_INVALID_PARA:
            return "MS_INVALID_PARA";
        case MS_MALLOC_FAIL:
            return "MS_MALLOC_FAIL";
        case MS_BUILT_ALREADY:
            return "MS_BUILT_ALREADY";
        case MS_FIND_FAIL:
            return "MS_FIND_FAIL";
        case MS_INIT_RWLOCK_FAIL:
            return "MS_INIT_RWLOCK_FAIL";
        case MS_MEMCPY_FAIL:
            return "MS_MEMCPY_FAIL";
        case MS_NOT_SUPPORTED:
            return "MS_NOT_SUPPORTED";
        case MS_STREAM_STALE:
            return "MS_STREAM_STALE";
        default:
            return "未知返回码";
    }
}

void demo_section(const char *name)
{
    printf("\n== %s ==\n", name);
}

int demo_step_ret(const char *name, int ret)
{
    printf("%-48s 返回值=%d (%s)\n", name, ret, demo_ret_name(ret));
    return ret;
}

void demo_note(const char *name, const char *value)
{
    printf("%-48s %s\n", name, value);
}

void demo_print_ids(const char *name, const uint32_t *ids, uint32_t count)
{
    uint32_t i;

    printf("%-48s 数量=%u ID=", name, count);
    for (i = 0; i < count; i++) {
        printf("%s%u", i == 0 ? "" : ",", ids[i]);
    }
    printf("\n");
}

ms_rule_t demo_make_rule(uint32_t alias, const char *key)
{
    ms_rule_t rule;

    rule.alias = alias;
    rule.key_len = (uint32_t)strlen(key);
    rule.key = (uint8_t *)key;
    return rule;
}

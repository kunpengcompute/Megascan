/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "demo_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ms_logical.h"

static const char *const g_risk_policy_apis[] = {
    "ms_logical_parse",
    "ms_logical_tree_destroy",
    "ms_logical_set",
    "ms_logical_get",
    "ms_logical_clear",
    "ms_logical_eval",
    "ms_logical_eval_inplace",
    NULL
};

const demo_scenario_t g_demo_scenario_risk_policy = {
    "risk-policy",
    "风险策略计算",
    "设置多个独立信号，并计算本地逻辑策略状态。",
    g_risk_policy_apis,
    demo_logical_apis
};

static int set_logical_values(uint8_t *vector, uint32_t vector_size,
                              const char *values)
{
    const char *cursor = values;

    while (*cursor != '\0') {
        char *end = NULL;
        unsigned long id = strtoul(cursor, &end, 10);
        uint8_t value;

        if (end == cursor || *end != '=' || (end[1] != '0' && end[1] != '1')) {
            printf("逻辑状态格式错误：%s\n", cursor);
            return MS_INVALID_PARA;
        }
        value = (uint8_t)(end[1] - '0');
        if (demo_step_ret("设置自定义逻辑状态",
                          ms_logical_set(vector, vector_size, (uint32_t)id, value)) != MS_SUCCESS) {
            return MS_INVALID_PARA;
        }
        cursor = end + 2;
        if (*cursor == ',') {
            cursor++;
        } else if (*cursor != '\0') {
            printf("逻辑状态格式错误：%s\n", cursor);
            return MS_INVALID_PARA;
        }
    }
    return MS_SUCCESS;
}

void demo_logical_apis(void)
{
    const demo_options_t *options = demo_get_options();
    const char *logical = options->logical_expression != NULL ?
                          options->logical_expression : "101&!102";
    ms_logical_tree_t tree;
    ms_logical_op_t inplace_ops[1];
    ms_logical_tree_t inplace_tree;
    uint8_t vector[512] = {0};
    uint8_t value = 0;
    uint8_t matched = 0;

    demo_section("风险策略计算");

    memset(&tree, 0, sizeof(tree));
    if (demo_step_ret("解析逻辑表达式", ms_logical_parse(logical, &tree)) != MS_SUCCESS) {
        return;
    }
    if (options->logical_values != NULL) {
        if (set_logical_values(vector, (uint32_t)DEMO_ARRAY_SIZE(vector),
                               options->logical_values) != MS_SUCCESS) {
            ms_logical_tree_destroy(&tree);
            return;
        }
    } else {
        (void)demo_step_ret("设置逻辑 key 101",
                            ms_logical_set(vector, (uint32_t)DEMO_ARRAY_SIZE(vector), 101, 1));
    }
    (void)demo_step_ret("读取逻辑 key 101",
                        ms_logical_get(vector, (uint32_t)DEMO_ARRAY_SIZE(vector), 101, &value));
    printf("%-48s 值=%u\n", "ms_logical_get(101) 输出", value);
    (void)demo_step_ret("计算逻辑表达式",
                        ms_logical_eval(&tree, vector, (uint32_t)DEMO_ARRAY_SIZE(vector), &matched));
    printf("%-48s 是否命中=%u\n", "ms_logical_eval 输出", matched);

    memset(&inplace_tree, 0, sizeof(inplace_tree));
    inplace_ops[0].id = 200;
    inplace_ops[0].op = MS_LOGICAL_OP_AND;
    inplace_ops[0].left = 101;
    inplace_ops[0].right = 102;
    inplace_tree.ops = inplace_ops;
    inplace_tree.op_count = 1;
    inplace_tree.op_capacity = 1;
    inplace_tree.result = 200;
    (void)demo_step_ret("设置逻辑 key 102",
                        ms_logical_set(vector, (uint32_t)DEMO_ARRAY_SIZE(vector), 102, 1));
    (void)demo_step_ret("原地计算逻辑表达式",
                        ms_logical_eval_inplace(&inplace_tree, vector,
                                                (uint32_t)DEMO_ARRAY_SIZE(vector), &matched));
    printf("%-48s 是否命中=%u\n", "ms_logical_eval_inplace 输出", matched);
    (void)demo_step_ret("清空逻辑向量", ms_logical_clear(vector, (uint32_t)DEMO_ARRAY_SIZE(vector)));
    ms_logical_tree_destroy(&tree);
    printf("%-48s 完成\n", "ms_logical_tree_destroy()");
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MEGASCAN_TEST_DEMO_COMMON_H
#define MEGASCAN_TEST_DEMO_COMMON_H

#include <stdint.h>

#include "ms_common.h"

#define DEMO_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    const char **expressions;
    uint32_t *ids;
    uint32_t *flags;
    ms_expr_ext_t *ext;
    uint32_t count;
} demo_expr_spec_t;

int demo_same(const char *left, const char *right);
int demo_parse_u32(const char *text, uint32_t *value);
int demo_parse_u64(const char *text, uint64_t *value);
int demo_parse_matcher(const char *text, ms_matcher_type *type);
const char *demo_ret_name(int ret);
int demo_result_from_ret(int ret);
int demo_parse_expr_spec(int argc, char **argv, int extended,
                         int ids_before_flags, demo_expr_spec_t *spec);
void demo_free_expr_spec(demo_expr_spec_t *spec);

void demo_list_api_tests(void);
void demo_print_api_cli_help(const char *prog);
void demo_print_api_test_help(const char *prog, const char *api);
int demo_run_api_cli(const char *api, int argc, char **argv);
void demo_list_modules(void);
int demo_show_module(const char *module);

#endif

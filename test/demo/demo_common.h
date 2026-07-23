/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MEGASCAN_TEST_DEMO_COMMON_H
#define MEGASCAN_TEST_DEMO_COMMON_H

#include <stdint.h>

#include "ms_common.h"

#define DEMO_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

void demo_section(const char *name);
int demo_step_ret(const char *name, int ret);
void demo_note(const char *name, const char *value);
void demo_print_ids(const char *name, const uint32_t *ids, uint32_t count);

void demo_list_api_tests(void);
void demo_print_api_cli_help(const char *prog);
void demo_print_api_test_help(const char *prog, const char *api);
int demo_run_api_cli(const char *api, int argc, char **argv);
void demo_list_modules(void);
int demo_show_module(const char *module);

#endif

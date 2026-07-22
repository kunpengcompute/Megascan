/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MEGASCAN_TEST_DEMO_COMMON_H
#define MEGASCAN_TEST_DEMO_COMMON_H

#include <stdint.h>

#include "ms_common.h"
#include "ms_rule.h"

#define DEMO_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef void (*demo_case_fn)(void);

typedef struct {
    const uint8_t *input;
    uint32_t input_len;
    const char *pattern;
    const char *key;
    const char *logical_expression;
    const char *logical_values;
    uint32_t chunk_size;
} demo_options_t;

typedef struct {
    const char *key;
    const char *title;
    const char *summary;
    const char *const *apis;
    demo_case_fn fn;
} demo_scenario_t;

extern const demo_scenario_t g_demo_scenario_runtime_config;
extern const demo_scenario_t g_demo_scenario_legacy_blacklist;
extern const demo_scenario_t g_demo_scenario_legacy_content_scan;
extern const demo_scenario_t g_demo_scenario_security_policy;
extern const demo_scenario_t g_demo_scenario_content_gateway;
extern const demo_scenario_t g_demo_scenario_stream_audit;
extern const demo_scenario_t g_demo_scenario_ioc_lookup;
extern const demo_scenario_t g_demo_scenario_risk_policy;

void demo_section(const char *name);
int demo_step_ret(const char *name, int ret);
void demo_note(const char *name, const char *value);
void demo_print_ids(const char *name, const uint32_t *ids, uint32_t count);
ms_rule_t demo_make_rule(uint32_t alias, const char *key);
const demo_options_t *demo_get_options(void);
void demo_set_options(const demo_options_t *options);

void demo_allocator_apis(void);
void demo_legacy_hash_handler_apis(void);
void demo_legacy_ac_rule_apis(void);
void demo_legacy_expression_apis(void);
void demo_database_block_apis(void);
void demo_database_stream_apis(void);
void demo_database_hash_apis(void);
void demo_logical_apis(void);

void demo_list_api_tests(void);
void demo_print_api_cli_help(const char *prog);
void demo_print_api_test_help(const char *prog, const char *api);
int demo_run_api_test(const char *api);
int demo_run_api_cli(const char *api, int argc, char **argv);
int demo_run_all_api_tests(void);
void demo_list_modules(void);
int demo_show_module(const char *module);
int demo_run_module_tests(const char *module);

#endif

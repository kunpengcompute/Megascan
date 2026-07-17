/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MEGASCAN_TEST_DEMO_BENCHMARK_CASE_H
#define MEGASCAN_TEST_DEMO_BENCHMARK_CASE_H

#include <stdint.h>

typedef struct {
    const char *algorithm;
    const char *mode;
    const char *rules_file;
    const char *corpus_file;
    uint32_t rule_count;
    uint32_t query_index;
    uint32_t chunk_size;
    int all_queries;
} demo_benchmark_case_t;

int demo_run_benchmark_case(const demo_benchmark_case_t *config);
int demo_run_rule_file_scan(const demo_benchmark_case_t *config,
                            const uint8_t *input, uint32_t input_len);

#endif

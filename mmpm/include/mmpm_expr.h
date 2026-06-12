/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_EXPR_H
#define MMPM_EXPR_H

#include "mmpm.h"

#define MMPM_EXPR_SUPPORTED_FLAGS (MMPM_FLAG_COMBINATION | MMPM_FLAG_QUIET)

int mmpm_compile_eval_combinations(mmpm_handler_t *handler,
                                   const uint32_t *aliases,
                                   uint32_t alias_count,
                                   uint32_t *comb_ids,
                                   uint32_t *comb_count,
                                   uint32_t comb_capacity);
int mmpm_compile_scan_ac_combinations(mmpm_handler_t *handler,
                                      const uint8_t *data,
                                      uint32_t len,
                                      uint32_t *comb_ids,
                                      uint32_t *comb_count,
                                      uint32_t comb_capacity);
void mmpm_compile_context_destroy(mmpm_handler_t *handler);

#endif

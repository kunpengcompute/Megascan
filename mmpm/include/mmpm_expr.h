/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_EXPR_H
#define MMPM_EXPR_H

#include "mmpm_handler.h"

#define MMPM_EXPR_SUPPORTED_FLAGS (MMPM_FLAG_COMBINATION | MMPM_FLAG_QUIET | MMPM_FLAG_SINGLEMATCH)

/**
 * Evaluate combination ids from alias hits.
 *
 * Compatibility note: this API belongs to the legacy handler path. It now
 * builds a database view from the handler and delegates to the database-native
 * combination evaluator.
 */
int mmpm_compile_eval_combinations(mmpm_handler_t *handler,
                                   const uint32_t *aliases,
                                   uint32_t alias_count,
                                   uint32_t *comb_ids,
                                   uint32_t *comb_count,
                                   uint32_t comb_capacity);
/**
 * Scan one block and return only matched combination ids.
 *
 * Compatibility note: this API belongs to the legacy handler path. It now
 * reuses the database-style block scan path and filters combination ids from
 * the reported matches.
 */
int mmpm_compile_scan_ac_combinations(mmpm_handler_t *handler,
                                      const uint8_t *data,
                                      uint32_t len,
                                      uint32_t *comb_ids,
                                      uint32_t *comb_count,
                                      uint32_t comb_capacity);
/**
 * Destroy the compile context owned by a legacy handler.
 */
void mmpm_compile_context_destroy(mmpm_handler_t *handler);

#endif

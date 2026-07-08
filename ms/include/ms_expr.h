/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_EXPR_H
#define MS_EXPR_H

#include "ms_handler.h"

#define MS_EXPR_SUPPORTED_FLAGS (MS_FLAG_COMBINATION | MS_FLAG_QUIET | MS_FLAG_SINGLEMATCH)

/**
 * Evaluate combination ids from alias hits.
 *
 * Compatibility note: this API belongs to the legacy handler path. It now
 * builds a database view from the handler and delegates to the database-native
 * combination evaluator.
 */
int ms_compile_eval_combinations(ms_handler_t *handler,
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
int ms_compile_scan_ac_combinations(ms_handler_t *handler,
                                      const uint8_t *data,
                                      uint32_t len,
                                      uint32_t *comb_ids,
                                      uint32_t *comb_count,
                                      uint32_t comb_capacity);
/**
 * Destroy the compile context owned by a legacy handler.
 */
void ms_compile_context_destroy(ms_handler_t *handler);

#endif

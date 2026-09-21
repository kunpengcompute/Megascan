/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_HANDLER_H
#define MS_HANDLER_H

#include "ms_common.h"

/**
 * Legacy aggregate handle.
 *
 * This object keeps the original mutable handler model: matcher instance,
 * compile context, matcher type, and build state are stored together.
 * It is still used by the compatibility compile/rule/runtime APIs.
 * New database-style scan and stream APIs should prefer ms_database_t,
 * ms_scratch_t and ms_stream_t.
 */
typedef struct {
  void *ms_matcher;
  void *ms_compile_ctx;
  ms_matcher_type type;

  /* mark whether the ac automation has been built. */
  bool is_built;
} ms_handler_t;

/**
 * Initialize multi-pattern matching handler.
 *
 * Compatibility note: this API initializes the legacy aggregate handle.
 * Database-style code should prefer ms_create_database() or
 * ms_compile_database_multi*().
 */
int ms_handler_init(ms_handler_t *handler, ms_matcher_type type,
                    uint32_t buckets, uint32_t secondary_buckets);

/**
 * Destroy multi-pattern matching handler.
 *
 * Compatibility note: this API destroys the legacy aggregate handle.
 * Database-style code should prefer ms_free_database().
 */
int ms_handler_destroy(ms_handler_t *handler);

#endif

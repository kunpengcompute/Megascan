/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_HANDLER_H
#define MMPM_HANDLER_H

#include "mmpm_common.h"

/**
 * Legacy aggregate handle.
 *
 * This object keeps the original mutable handler model: matcher instance,
 * compile context, matcher type, and build state are stored together.
 * It is still used by the compatibility compile/rule/runtime APIs.
 * New database-style scan and stream APIs should prefer mmpm_database_t,
 * mmpm_scratch_t and ms_stream_t.
 */
typedef struct {
    void *mmpm_matcher;
    void *mmpm_compile_ctx;
    mmpm_matcher_type type;

    /* mark whether the ac automation has been built. */
    bool is_built;
} mmpm_handler_t;

/**
 * Initialize multi-pattern matching handler.
 *
 * Compatibility note: this API initializes the legacy aggregate handle.
 * Database-style code should prefer mmpm_create_database() or
 * mmpm_compile_database_multi*().
 */
int mmpm_handler_init(mmpm_handler_t *handler, mmpm_matcher_type type,
                      uint32_t buckets, uint32_t secondary_buckets);

/**
 * Destroy multi-pattern matching handler.
 *
 * Compatibility note: this API destroys the legacy aggregate handle.
 * Database-style code should prefer mmpm_free_database().
 */
int mmpm_handler_destroy(mmpm_handler_t *handler);

#endif

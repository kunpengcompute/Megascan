/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_DATABASE_H
#define MMPM_DATABASE_H

#include "mmpm_common.h"

typedef struct mmpm_database mmpm_database_t;

int MMPM_CDECL mmpm_create_database(mmpm_database_t **db, mmpm_matcher_type type,
                                    uint32_t buckets, uint32_t secondary_buckets);

int MMPM_CDECL mmpm_compile_database_multi(mmpm_database_t **db,
                                           mmpm_matcher_type type,
                                           uint32_t buckets,
                                           uint32_t secondary_buckets,
                                           const char *const *expressions,
                                           const uint32_t *flags,
                                           const uint32_t *ids,
                                           uint32_t elements);

int MMPM_CDECL mmpm_compile_database_multi_ext(mmpm_database_t **db,
                                               mmpm_matcher_type type,
                                               uint32_t buckets,
                                               uint32_t secondary_buckets,
                                               const char *const *expressions,
                                               const uint32_t *flags,
                                               const uint32_t *ids,
                                               const mmpm_expr_ext_t *ext,
                                               uint32_t elements);

int MMPM_CDECL mmpm_free_database(mmpm_database_t *db);

mmpm_handler_t *mmpm_database_get_handler(mmpm_database_t *db);

#endif

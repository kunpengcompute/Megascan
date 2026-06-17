/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_DATABASE_INTERNAL_H
#define MMPM_DATABASE_INTERNAL_H

#include "mmpm_common.h"

struct mmpm_database {
    void *mmpm_matcher;
    void *mmpm_compile_ctx;
    mmpm_matcher_type type;
    bool is_built;
};

void mmpm_database_to_handler(const mmpm_database_t *db, mmpm_handler_t *handler);
void mmpm_database_from_handler(mmpm_database_t *db, const mmpm_handler_t *handler);

int mmpm_scan_handler_database(mmpm_handler_t *handler,
                               const uint8_t *data,
                               uint32_t len,
                               uint32_t *ids,
                               uint32_t *id_count,
                               uint32_t id_capacity);

#endif

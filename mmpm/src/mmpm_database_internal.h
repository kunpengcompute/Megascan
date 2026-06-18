/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_DATABASE_INTERNAL_H
#define MMPM_DATABASE_INTERNAL_H

#include "mmpm_database.h"
#include "mmpm_handler.h"

struct mmpm_database {
    void *mmpm_matcher;
    void *mmpm_compile_ctx;
    mmpm_matcher_type type;
    bool is_built;
};

void mmpm_database_from_handler(mmpm_database_t *db, const mmpm_handler_t *handler);

#endif

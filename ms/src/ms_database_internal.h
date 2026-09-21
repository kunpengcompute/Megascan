/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_DATABASE_INTERNAL_H
#define MS_DATABASE_INTERNAL_H

#include "ms_database.h"
#include "ms_handler.h"

typedef enum {
    MS_DATABASE_BACKEND_NATIVE = 0,
    MS_DATABASE_BACKEND_LEGACY_AC,
    MS_DATABASE_BACKEND_COMPACT_AC
} ms_database_backend_t;

struct ms_database {
    void *ms_matcher;
    void *ms_compile_ctx;
    ms_matcher_type type;
    ms_database_backend_t backend;
    bool is_built;
};

void ms_database_from_handler(ms_database_t *db, const ms_handler_t *handler);

#endif

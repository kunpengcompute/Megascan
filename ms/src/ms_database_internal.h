/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_DATABASE_INTERNAL_H
#define MS_DATABASE_INTERNAL_H

#include "ms_database.h"
#include "ms_handler.h"

struct ms_database {
    void *ms_matcher;
    void *ms_compile_ctx;
    ms_matcher_type type;
    bool is_built;
};

void ms_database_from_handler(ms_database_t *db, const ms_handler_t *handler);

#endif

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "mmpm_database.h"
#include "mmpm_allocator.h"
#include "mmpm_compile.h"

struct mmpm_database {
    mmpm_handler_t handler;
};

static bool mmpm_database_has_combination_rules(const uint32_t *flags, uint32_t elements)
{
    uint32_t i;

    if (flags == NULL) {
        return false;
    }

    for (i = 0; i < elements; i++) {
        if ((flags[i] & MMPM_FLAG_COMBINATION) != 0) {
            return true;
        }
    }

    return false;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_create_database(mmpm_database_t **db, mmpm_matcher_type type,
                                    uint32_t buckets, uint32_t secondary_buckets)
{
    mmpm_database_t *new_db;
    int ret;

    if (db == NULL || *db != NULL ||
        (type != MMPM_MATCHER_TYPE_HASH && type != MMPM_MATCHER_TYPE_AC)) {
        return MMPM_INVALID_PARA;
    }

    new_db = (mmpm_database_t *)mmpm_database_calloc(1, sizeof(mmpm_database_t));
    if (new_db == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    ret = mmpm_handler_init(&new_db->handler, type, buckets, secondary_buckets);
    if (ret != MMPM_SUCCESS) {
        mmpm_database_free(new_db);
        return ret;
    }

    *db = new_db;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_compile_database_multi(mmpm_database_t **db,
                                           mmpm_matcher_type type,
                                           uint32_t buckets,
                                           uint32_t secondary_buckets,
                                           const char *const *expressions,
                                           const uint32_t *flags,
                                           const uint32_t *ids,
                                           uint32_t elements)
{
    return mmpm_compile_database_multi_ext(db, type, buckets, secondary_buckets,
                                           expressions, flags, ids, NULL, elements);
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_compile_database_multi_ext(mmpm_database_t **db,
                                               mmpm_matcher_type type,
                                               uint32_t buckets,
                                               uint32_t secondary_buckets,
                                               const char *const *expressions,
                                               const uint32_t *flags,
                                               const uint32_t *ids,
                                               const mmpm_expr_ext_t *ext,
                                               uint32_t elements)
{
    mmpm_database_t *new_db = NULL;
    int ret;

    if (db == NULL || *db != NULL) {
        return MMPM_INVALID_PARA;
    }

    if (type != MMPM_MATCHER_TYPE_HASH && type != MMPM_MATCHER_TYPE_AC) {
        return MMPM_INVALID_PARA;
    }

    if (type != MMPM_MATCHER_TYPE_AC && mmpm_database_has_combination_rules(flags, elements)) {
        return MMPM_NOT_SUPPORTED;
    }

    ret = mmpm_create_database(&new_db, type, buckets, secondary_buckets);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = mmpm_compile_multi_ext(&new_db->handler, expressions, flags, ids, ext, elements);
    if (ret != MMPM_SUCCESS) {
        (void)mmpm_free_database(new_db);
        return ret;
    }

    *db = new_db;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_free_database(mmpm_database_t *db)
{
    int ret;

    if (db == NULL) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_handler_destroy(&db->handler);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    mmpm_database_free(db);
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
mmpm_handler_t *mmpm_database_get_handler(mmpm_database_t *db)
{
    if (db == NULL) {
        return NULL;
    }

    return &db->handler;
}

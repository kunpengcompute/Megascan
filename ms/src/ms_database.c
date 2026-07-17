/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "ms_database.h"
#include "ms_ac.h"
#include "ms_acdb.h"
#include "ms_allocator.h"
#include "ms_compile.h"
#include "ms_hash.h"
#include "ms_rule.h"
#include "ms_rule_internal.h"
#include "ms_database_internal.h"
#include "expr/ms_expr_internal.h"

#define HASH_DEFAULT_BUCKET_SIZE 100000
#define IPV4_TUPLE_SIZE          16

static bool ms_database_has_combination_rules(const uint32_t *flags, uint32_t elements)
{
    uint32_t i;

    if (flags == NULL) {
        return false;
    }

    for (i = 0; i < elements; i++) {
        if ((flags[i] & MS_FLAG_COMBINATION) != 0) {
            return true;
        }
    }

    return false;
}

void ms_database_from_handler(ms_database_t *db, const ms_handler_t *handler)
{
    if (db == NULL || handler == NULL) {
        return;
    }

    db->ms_matcher = handler->ms_matcher;
    db->ms_compile_ctx = handler->ms_compile_ctx;
    db->type = handler->type;
    db->backend = handler->type == MS_MATCHER_TYPE_AC
                      ? MS_DATABASE_BACKEND_LEGACY_AC
                      : MS_DATABASE_BACKEND_NATIVE;
    db->is_built = handler->is_built;
}

static int ms_database_init_state(ms_database_t *db, ms_matcher_type type,
                                     uint32_t buckets, uint32_t secondary_buckets)
{
    int ret = MS_SUCCESS;

    if (db == NULL || type >= MS_MATCHER_TYPE_MAX || type == MS_MATCHER_NONE) {
        return MS_INVALID_PARA;
    }

    if (db->ms_matcher != NULL) {
        return MS_BUILT_ALREADY;
    }

    switch (type) {
        case MS_MATCHER_TYPE_HASH:
            buckets = buckets ? buckets : HASH_DEFAULT_BUCKET_SIZE;
            ret = hash_table_create((ms_hash **)&db->ms_matcher, buckets, IPV4_TUPLE_SIZE);
            if (ret != MS_SUCCESS) {
                return ret;
            }
            ret = hash_table_build(db->ms_matcher, secondary_buckets);
            if (ret != MS_SUCCESS) {
                (void)hash_table_destroy(db->ms_matcher);
                db->ms_matcher = NULL;
                return ret;
            }
            break;
        case MS_MATCHER_TYPE_AC:
            ret = ms_acdb_create((ms_acdb_t **)&db->ms_matcher);
            db->backend = MS_DATABASE_BACKEND_COMPACT_AC;
            break;
        default:
            return MS_INVALID_PARA;
    }

    if (ret != MS_SUCCESS) {
        ms_database_free(db->ms_matcher);
        db->ms_matcher = NULL;
        return ret;
    }

    db->type = type;
    db->is_built = false;
    return MS_SUCCESS;
}

static int ms_database_destroy_state(ms_database_t *db)
{
    if (db == NULL) {
        return MS_INVALID_PARA;
    }

    if (db->ms_compile_ctx != NULL) {
        ms_compile_context_destroy_database(db);
    }

    if (db->ms_matcher == NULL) {
        return MS_SUCCESS;
    }

    switch (db->type) {
        case MS_MATCHER_TYPE_HASH:
            (void)hash_table_destroy(db->ms_matcher);
            break;
        case MS_MATCHER_TYPE_AC:
            if (db->backend == MS_DATABASE_BACKEND_COMPACT_AC) {
                ms_acdb_destroy((ms_acdb_t *)db->ms_matcher);
            } else {
                (void)ac_automaton_destroy(db->ms_matcher);
                ms_database_free(db->ms_matcher);
            }
            break;
        default:
            return MS_INVALID_PARA;
    }

    db->ms_matcher = NULL;
    db->type = MS_MATCHER_NONE;
    db->backend = MS_DATABASE_BACKEND_NATIVE;
    db->is_built = false;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int MS_CDECL ms_create_database(ms_database_t **db, ms_matcher_type type,
                                    uint32_t buckets, uint32_t secondary_buckets)
{
    ms_database_t *new_db;
    int ret;

    if (db == NULL || *db != NULL ||
        (type != MS_MATCHER_TYPE_HASH && type != MS_MATCHER_TYPE_AC)) {
        return MS_INVALID_PARA;
    }

    new_db = (ms_database_t *)ms_database_calloc(1, sizeof(ms_database_t));
    if (new_db == NULL) {
        return MS_MALLOC_FAIL;
    }

    ret = ms_database_init_state(new_db, type, buckets, secondary_buckets);
    if (ret != MS_SUCCESS) {
        ms_database_free(new_db);
        return ret;
    }

    *db = new_db;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int MS_CDECL ms_compile_database_multi(ms_database_t **db,
                                           ms_matcher_type type,
                                           uint32_t buckets,
                                           uint32_t secondary_buckets,
                                           const char *const *expressions,
                                           const uint32_t *flags,
                                           const uint32_t *ids,
                                           uint32_t elements)
{
    return ms_compile_database_multi_ext(db, type, buckets, secondary_buckets,
                                           expressions, flags, ids, NULL, elements);
}

__attribute__((visibility("default")))
int MS_CDECL ms_compile_database_multi_ext(ms_database_t **db,
                                               ms_matcher_type type,
                                               uint32_t buckets,
                                               uint32_t secondary_buckets,
                                               const char *const *expressions,
                                               const uint32_t *flags,
                                               const uint32_t *ids,
                                               const ms_expr_ext_t *ext,
                                               uint32_t elements)
{
    ms_database_t *new_db = NULL;
    int ret;

    if (db == NULL || *db != NULL) {
        return MS_INVALID_PARA;
    }

    if (type != MS_MATCHER_TYPE_HASH && type != MS_MATCHER_TYPE_AC) {
        return MS_INVALID_PARA;
    }

    if (type != MS_MATCHER_TYPE_AC && ms_database_has_combination_rules(flags, elements)) {
        return MS_NOT_SUPPORTED;
    }

    ret = ms_create_database(&new_db, type, buckets, secondary_buckets);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    ret = ms_compile_database_multi_native(new_db, expressions, flags, ids, ext, elements);
    if (ret != MS_SUCCESS) {
        (void)ms_free_database(new_db);
        return ret;
    }

    *db = new_db;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int MS_CDECL ms_find_database_rule(const ms_database_t *db, ms_rule_t *rule)
{
    if (db == NULL || rule == NULL) {
        return MS_INVALID_PARA;
    }

    return ms_rule_find_database_direct(db, rule);
}

__attribute__((visibility("default")))
int MS_CDECL ms_free_database(ms_database_t *db)
{
    int ret;

    if (db == NULL) {
        return MS_INVALID_PARA;
    }

    ret = ms_database_destroy_state(db);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    ms_database_free(db);
    return MS_SUCCESS;
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "mmpm_database.h"
#include "mmpm_ac.h"
#include "mmpm_allocator.h"
#include "mmpm_compile.h"
#include "mmpm_hash.h"
#include "mmpm_rule.h"
#include "mmpm_rule_internal.h"
#include "mmpm_database_internal.h"
#include "expr/mmpm_expr_internal.h"

#define HASH_DEFAULT_BUCKET_SIZE 100000
#define IPV4_TUPLE_SIZE          16

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

void mmpm_database_from_handler(mmpm_database_t *db, const mmpm_handler_t *handler)
{
    if (db == NULL || handler == NULL) {
        return;
    }

    db->mmpm_matcher = handler->mmpm_matcher;
    db->mmpm_compile_ctx = handler->mmpm_compile_ctx;
    db->type = handler->type;
    db->is_built = handler->is_built;
}

static int mmpm_database_init_state(mmpm_database_t *db, mmpm_matcher_type type,
                                    uint32_t buckets, uint32_t secondary_buckets)
{
    int ret = MMPM_SUCCESS;

    if (db == NULL || type >= MMPM_MATCHER_TYPE_MAX || type == MMPM_MATCHER_NONE) {
        return MMPM_INVALID_PARA;
    }

    if (db->mmpm_matcher != NULL) {
        return MMPM_BUILT_ALREADY;
    }

    switch (type) {
        case MMPM_MATCHER_TYPE_HASH:
            buckets = buckets ? buckets : HASH_DEFAULT_BUCKET_SIZE;
            ret = hash_table_create((mmpm_hash **)&db->mmpm_matcher, buckets, IPV4_TUPLE_SIZE);
            if (ret != MMPM_SUCCESS) {
                return ret;
            }
            ret = hash_table_build(db->mmpm_matcher, secondary_buckets);
            break;
        case MMPM_MATCHER_TYPE_AC:
            db->mmpm_matcher = mmpm_database_malloc(sizeof(ACAutomaton));
            if (db->mmpm_matcher == NULL) {
                return MMPM_MALLOC_FAIL;
            }
            ret = ac_automaton_init(db->mmpm_matcher);
            break;
        default:
            return MMPM_INVALID_PARA;
    }

    if (ret != MMPM_SUCCESS) {
        mmpm_database_free(db->mmpm_matcher);
        db->mmpm_matcher = NULL;
        return ret;
    }

    db->type = type;
    db->is_built = false;
    return MMPM_SUCCESS;
}

static int mmpm_database_destroy_state(mmpm_database_t *db)
{
    if (db == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (db->mmpm_compile_ctx != NULL) {
        mmpm_compile_context_destroy_database(db);
    }

    if (db->mmpm_matcher == NULL) {
        return MMPM_SUCCESS;
    }

    switch (db->type) {
        case MMPM_MATCHER_TYPE_HASH:
            (void)hash_table_destroy(db->mmpm_matcher);
            break;
        case MMPM_MATCHER_TYPE_AC:
            (void)ac_automaton_destroy(db->mmpm_matcher);
            mmpm_database_free(db->mmpm_matcher);
            break;
        default:
            return MMPM_INVALID_PARA;
    }

    db->mmpm_matcher = NULL;
    db->type = MMPM_MATCHER_NONE;
    db->is_built = false;
    return MMPM_SUCCESS;
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

    ret = mmpm_database_init_state(new_db, type, buckets, secondary_buckets);
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

    ret = mmpm_compile_database_multi_native(new_db, expressions, flags, ids, ext, elements);
    if (ret != MMPM_SUCCESS) {
        (void)mmpm_free_database(new_db);
        return ret;
    }

    *db = new_db;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_find_database_rule(const mmpm_database_t *db, mmpm_rule_t *rule)
{
    if (db == NULL || rule == NULL) {
        return MMPM_INVALID_PARA;
    }

    return mmpm_rule_find_database_direct(db, rule);
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_free_database(mmpm_database_t *db)
{
    int ret;

    if (db == NULL) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_database_destroy_state(db);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    mmpm_database_free(db);
    return MMPM_SUCCESS;
}

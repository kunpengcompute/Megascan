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
int ms_create_database(ms_database_t **db, ms_matcher_type type,
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
int ms_compile_database_multi(ms_database_t **db,
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
int ms_compile_database_multi_ext(ms_database_t **db,
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
int ms_find_database_rule(const ms_database_t *db, ms_rule_t *rule)
{
    if (db == NULL || rule == NULL) {
        return MS_INVALID_PARA;
    }

    return ms_rule_find_database_direct(db, rule);
}

static bool ms_database_remove_rule_is_valid(const ms_rule_t *rule)
{
    return rule != NULL &&
           ((rule->key != NULL && rule->key_len != 0) ||
            (rule->key == NULL && rule->key_len == 0));
}

static int ms_remove_database_rules_atomic(ms_database_t *db,
                                           const ms_rule_t *rules,
                                           uint32_t rule_num)
{
    ms_compile_context_t *old_ctx =
        (ms_compile_context_t *)db->ms_compile_ctx;
    ms_compile_context_t *new_ctx = NULL;
    ms_acdb_t *candidate = NULL;
    ms_rule_t *plain_rules = NULL;
    uint32_t plain_count = 0;
    uint32_t i;
    int ret;

    /* Build both candidates before changing the live database. */
    ret = ms_compile_context_clone(old_ctx, &new_ctx);
    if (ret != MS_SUCCESS) {
        return ret;
    }
    plain_rules = (ms_rule_t *)ms_calloc(rule_num, sizeof(ms_rule_t));
    if (plain_rules == NULL) {
        ms_compile_context_free(new_ctx);
        return MS_MALLOC_FAIL;
    }

    for (i = 0; i < rule_num; i++) {
        uint32_t j;

        for (j = 0; j < i; j++) {
            if (rules[j].alias == rules[i].alias) {
                ret = MS_INVALID_PARA;
                goto fail;
            }
        }
        if (rules[i].key != NULL) {
            uint32_t lkey;

            ret = ms_compile_context_find_lkey(old_ctx, rules[i].alias, &lkey);
            if (ret != MS_SUCCESS) {
                goto fail;
            }
            plain_rules[plain_count].alias = lkey;
            plain_rules[plain_count].key = rules[i].key;
            plain_rules[plain_count].key_len = rules[i].key_len;
            plain_count++;
        } else if (!ms_compile_context_has_comb_id(old_ctx, rules[i].alias)) {
            ret = MS_FIND_FAIL;
            goto fail;
        }
    }

    if (plain_count != 0) {
        ret = ms_acdb_prepare_remove((const ms_acdb_t *)db->ms_matcher,
                                     plain_rules, plain_count, &candidate);
        if (ret != MS_SUCCESS) {
            goto fail;
        }
    }

    for (i = 0; i < rule_num; i++) {
        if (rules[i].key == NULL) {
            continue;
        }
        ret = ms_compile_context_remove_id(new_ctx, rules[i].alias);
        if (ret != MS_SUCCESS) {
            goto fail;
        }
    }
    for (i = 0; i < rule_num; i++) {
        if (rules[i].key != NULL) {
            continue;
        }
        ret = ms_compile_context_remove_comb_id(new_ctx, rules[i].alias);
        if (ret != MS_SUCCESS && ret != MS_FIND_FAIL) {
            goto fail;
        }
    }

    db->ms_compile_ctx = new_ctx;
    if (candidate != NULL) {
        ms_acdb_commit_rebuild((ms_acdb_t *)db->ms_matcher, candidate);
    } else {
        ms_acdb_bump_generation((ms_acdb_t *)db->ms_matcher);
    }
    ms_compile_context_free(old_ctx);
    ms_free(plain_rules);
    return MS_SUCCESS;

fail:
    ms_acdb_destroy(candidate);
    ms_compile_context_free(new_ctx);
    ms_free(plain_rules);
    return ret;
}

__attribute__((visibility("default")))
int ms_remove_database_rule(ms_database_t *db,
                                      const ms_rule_t *rule,
                                      uint32_t rule_num)
{
    uint32_t i;
    int ret;

    if (db == NULL || rule == NULL || rule_num == 0) {
        return MS_INVALID_PARA;
    }
    if (db->type == MS_MATCHER_TYPE_HASH) {
        return MS_NOT_SUPPORTED;
    }
    if (db->type != MS_MATCHER_TYPE_AC ||
        db->backend != MS_DATABASE_BACKEND_COMPACT_AC ||
        db->ms_matcher == NULL || db->ms_compile_ctx == NULL ||
        !db->is_built) {
        return MS_INVALID_PARA;
    }
    for (i = 0; i < rule_num; i++) {
        if (!ms_database_remove_rule_is_valid(&rule[i])) {
            return MS_INVALID_PARA;
        }
    }
    ret = ms_remove_database_rules_atomic(db, rule, rule_num);
    return ret;
}

__attribute__((visibility("default")))
int ms_free_database(ms_database_t *db)
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

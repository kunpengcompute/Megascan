/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdio.h>
#include <string.h>

#include "ms_allocator.h"
#include "ms_compile.h"
#include "ms_runtime.h"
#include "ms_rule.h"
#include "ms_rule_internal.h"
#include "ms_hash.h"
#include "ms_ac.h"
#include "ms_acdb.h"
#include "expr/ms_expr_internal.h"

static int ms_database_is_valid(const ms_database_t *db, bool is_add)
{
    if (db == NULL) {
        return MS_INVALID_PARA;
    }

    if (db->ms_matcher && db->type < MS_MATCHER_TYPE_MAX) {
        return MS_SUCCESS;
    }

    if (is_add && db->ms_matcher == NULL && db->type == MS_MATCHER_NONE) {
        return MS_SUCCESS;
    }

    return MS_INVALID_PARA;
}

static int ms_handler_is_valid(ms_handler_t *handler, bool is_add)
{
    if (handler == NULL) {
        return MS_INVALID_PARA;
    }

    if (handler->ms_matcher && handler->type < MS_MATCHER_TYPE_MAX) {
        return MS_SUCCESS;
    }

    if (is_add && handler->ms_matcher == NULL && handler->type == MS_MATCHER_NONE) {
        return MS_SUCCESS;
    }

    return MS_INVALID_PARA;
}

int ms_rule_add_direct(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;

    if (ms_handler_is_valid(handler, true) || rule == NULL) {
        return MS_INVALID_PARA;
    }

    if (handler->ms_matcher == NULL && handler->type == MS_MATCHER_NONE) {
        printf("first import the rules need choose hash or ac.\n");
        return MS_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MS_MATCHER_TYPE_HASH:
                ret = hash_table_add(handler->ms_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                /*
                 * Batch hash insertion keeps the already inserted part on error
                 * and returns the failure to the caller.
                 */
                if (ret) {
                    return ret;
                }
                break;
            case MS_MATCHER_TYPE_AC:
                if (handler->is_built) {
                    return MS_BUILT_ALREADY;
                }
                ret = ac_automaton_insert(handler->ms_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    ac_automaton_destroy(handler->ms_matcher);
                    return ret;
                }
                break;
            default:
                break;
        }
    }

    if (handler->type == MS_MATCHER_TYPE_AC) {
        ret = ac_automaton_build(handler->ms_matcher);
        if (ret) {
            (void)ac_automaton_destroy(handler->ms_matcher);
        } else {
            handler->is_built = true;
        }
    }

    return ret;
}

int ms_rule_add_database_direct(ms_database_t *db, ms_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;

    if (ms_database_is_valid(db, true) || rule == NULL) {
        return MS_INVALID_PARA;
    }

    if (db->ms_matcher == NULL && db->type == MS_MATCHER_NONE) {
        printf("first import the rules need choose hash or ac.\n");
        return MS_INVALID_PARA;
    }

    if (db->type == MS_MATCHER_TYPE_AC &&
        db->backend == MS_DATABASE_BACKEND_COMPACT_AC) {
        if (db->is_built) {
            return MS_BUILT_ALREADY;
        }
        ret = ms_acdb_build((ms_acdb_t *)db->ms_matcher, rule, rule_num);
        if (ret == MS_SUCCESS) {
            db->is_built = true;
        }
        return ret;
    }

    for (i = 0; i < rule_num; i++) {
        switch (db->type) {
            case MS_MATCHER_TYPE_HASH:
                ret = hash_table_add(db->ms_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    return ret;
                }
                break;
            case MS_MATCHER_TYPE_AC:
                if (db->is_built) {
                    return MS_BUILT_ALREADY;
                }
                ret = ac_automaton_insert(db->ms_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    ac_automaton_destroy(db->ms_matcher);
                    return ret;
                }
                break;
            default:
                break;
        }
    }

    if (db->type == MS_MATCHER_TYPE_AC) {
        ret = ac_automaton_build(db->ms_matcher);
        if (ret) {
            (void)ac_automaton_destroy(db->ms_matcher);
        } else {
            db->is_built = true;
        }
    }

    return ret;
}

int ms_rule_find_direct(ms_handler_t *handler, ms_rule_t *rule)
{
    if (rule == NULL || rule->key == NULL || ms_handler_is_valid(handler, false)) {
        return MS_INVALID_PARA;
    }

    switch (handler->type) {
        case MS_MATCHER_TYPE_HASH:
            return hash_table_find(handler->ms_matcher, rule->key, rule->key_len, &(rule->alias));
        case MS_MATCHER_TYPE_AC:
            if (!handler->is_built) {
                return MS_INVALID_PARA;
            }
            return ac_automaton_match(handler->ms_matcher, rule->key, rule->key_len);
        default:
            return MS_INVALID_PARA;
    }
}

int ms_rule_find_database_direct(const ms_database_t *db, ms_rule_t *rule)
{
    if (rule == NULL || rule->key == NULL || ms_database_is_valid(db, false)) {
        return MS_INVALID_PARA;
    }

    switch (db->type) {
        case MS_MATCHER_TYPE_HASH:
            return hash_table_find(db->ms_matcher, rule->key, rule->key_len, &(rule->alias));
        case MS_MATCHER_TYPE_AC:
            if (!db->is_built) {
                return MS_INVALID_PARA;
            }
            if (db->backend == MS_DATABASE_BACKEND_COMPACT_AC) {
                return ms_acdb_match((const ms_acdb_t *)db->ms_matcher,
                                     rule->key, rule->key_len);
            }
            return ac_automaton_match(db->ms_matcher, rule->key, rule->key_len);
        default:
            return MS_INVALID_PARA;
    }
}

int ms_rule_remove_direct(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = MS_SUCCESS;

    if (rule == NULL || rule_num == 0 || ms_handler_is_valid(handler, false)) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MS_MATCHER_TYPE_HASH:
                ret = hash_table_remove(handler->ms_matcher, rule[i].key, rule[i].key_len);
                if (ret != MS_SUCCESS) {
                    return ret;
                }
                break;
            case MS_MATCHER_TYPE_AC:
                if (!handler->is_built) {
                    return MS_INVALID_PARA;
                }
                ret = ac_automaton_remove(handler->ms_matcher, rule[i].alias,
                                          rule[i].key, rule[i].key_len);
                if (ret != MS_SUCCESS) {
                    return ret;
                }
                break;
            default:
                return MS_INVALID_PARA;
        }
    }

    return ret;
}

static bool ms_rule_has_key(const ms_rule_t *rule)
{
    return rule != NULL && rule->key != NULL && rule->key_len != 0;
}

static bool ms_rule_is_combination_remove(const ms_rule_t *rule)
{
    return rule != NULL && rule->key == NULL && rule->key_len == 0;
}

static bool ms_rule_remove_param_is_valid(const ms_rule_t *rule)
{
    return ms_rule_has_key(rule) || ms_rule_is_combination_remove(rule);
}

static void ms_rule_free_exprs(char **exprs, uint32_t count)
{
    uint32_t i;

    if (exprs == NULL) {
        return;
    }

    for (i = 0; i < count; i++) {
        if (exprs[i] != NULL) {
            ms_free(exprs[i]);
        }
    }
    ms_free(exprs);
}

static int ms_rule_to_exprs(ms_rule_t *rule,
                              uint32_t rule_num,
                              char ***exprs_out,
                              uint32_t **ids_out)
{
    char **exprs = NULL;
    uint32_t *ids = NULL;
    uint32_t i;

    if (rule == NULL || rule_num == 0 || exprs_out == NULL || ids_out == NULL) {
        return MS_INVALID_PARA;
    }

    exprs = (char **)ms_calloc(rule_num, sizeof(char *));
    ids = (uint32_t *)ms_calloc(rule_num, sizeof(uint32_t));
    if (exprs == NULL || ids == NULL) {
        ms_rule_free_exprs(exprs, rule_num);
        if (ids != NULL) {
            ms_free(ids);
        }
        return MS_MALLOC_FAIL;
    }

    for (i = 0; i < rule_num; i++) {
        if (rule[i].key == NULL || rule[i].key_len == 0 ||
            memchr(rule[i].key, '\0', rule[i].key_len) != NULL) {
            ms_rule_free_exprs(exprs, rule_num);
            ms_free(ids);
            return MS_INVALID_PARA;
        }

        exprs[i] = (char *)ms_calloc(rule[i].key_len + 1, sizeof(char));
        if (exprs[i] == NULL) {
            ms_rule_free_exprs(exprs, rule_num);
            ms_free(ids);
            return MS_MALLOC_FAIL;
        }
        (void)memcpy(exprs[i], rule[i].key, rule[i].key_len);
        ids[i] = rule[i].alias;
    }

    *exprs_out = exprs;
    *ids_out = ids;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_add_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num)
{
    char **exprs = NULL;
    uint32_t *ids = NULL;
    int ret;

    if (handler == NULL) {
        return MS_INVALID_PARA;
    }

    if (handler->type == MS_MATCHER_TYPE_HASH) {
        return ms_rule_add_direct(handler, rule, rule_num);
    }

    ret = ms_rule_to_exprs(rule, rule_num, &exprs, &ids);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    ret = ms_compile_multi_ext(handler, (const char *const *)exprs, NULL, ids, NULL, rule_num);

    ms_rule_free_exprs(exprs, rule_num);
    ms_free(ids);
    return ret;
}

__attribute__((visibility("default")))
int ms_find_rule(ms_handler_t *handler, ms_rule_t *rule)
{
    ms_compile_context_t *ctx;
    uint32_t *ids = NULL;
    uint32_t id_count = 0;
    uint32_t id_capacity;
    int ret = MS_SUCCESS;

    if (rule == NULL || rule->key == NULL || ms_handler_is_valid(handler, false)) {
        return MS_INVALID_PARA;
    }

    switch (handler->type) {
        case MS_MATCHER_TYPE_HASH:
            ret = ms_rule_find_direct(handler, rule);
            break;
        case MS_MATCHER_TYPE_AC:
            if (!handler->is_built) {
                return MS_INVALID_PARA;
            }
            ctx = (ms_compile_context_t *)handler->ms_compile_ctx;
            if (ctx == NULL) {
                return MS_INVALID_PARA;
            }

            id_capacity = ctx->expr_count + ctx->comb_count;
            if (id_capacity != 0) {
                ids = (uint32_t *)ms_calloc(id_capacity, sizeof(uint32_t));
                if (ids == NULL) {
                    return MS_MALLOC_FAIL;
                }
            }

            ret = ms_scan_expr(handler, rule->key, rule->key_len, ids, &id_count, id_capacity);
            if (ret == MS_SUCCESS && id_count != 0) {
                rule->alias = ids[0];
            }
            if (ids != NULL) {
                ms_free(ids);
            }
            break;
        default:
            break;
    }

    return ret;
}

__attribute__((visibility("default")))
int ms_remove_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;
    bool removed_from_ctx = false;

    if (rule == NULL || rule_num == 0 || ms_handler_is_valid(handler, false)) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        if (!ms_rule_remove_param_is_valid(&rule[i])) {
            return MS_INVALID_PARA;
        }
    }

    for (i = 0; i < rule_num; i++) {
        bool has_key = ms_rule_has_key(&rule[i]);

        if (has_key) {
            ret = ms_rule_remove_direct(handler, &rule[i], 1);
            if (ret != MS_SUCCESS) {
                goto end;
            }
        }

        if (has_key) {
            ret = ms_compile_context_remove_id((ms_compile_context_t *)handler->ms_compile_ctx, rule[i].alias);
        } else {
            ret = ms_compile_context_remove_comb_id((ms_compile_context_t *)handler->ms_compile_ctx,
                                                      rule[i].alias);
        }
        if (ret == MS_SUCCESS) {
            removed_from_ctx = true;
            continue;
        }
        if (!has_key || ret != MS_FIND_FAIL) {
            goto end;
        }
        ret = MS_SUCCESS;
    }

    if (removed_from_ctx && handler->type == MS_MATCHER_TYPE_AC && handler->ms_matcher != NULL) {
        ((ACAutomaton *)handler->ms_matcher)->generation++;
    }

end:
    return ret;
}

__attribute__((visibility("default")))
int ms_set_hash_func(ms_handler_t *handler, hash_calc_func hash_func)
{
    if (handler == NULL || handler->ms_matcher == NULL ||
        handler->type != MS_MATCHER_TYPE_HASH || hash_func == NULL) {
        return MS_INVALID_PARA;
    }

    hash_table_set_hash_func(handler->ms_matcher, hash_func);
    return MS_SUCCESS;
}

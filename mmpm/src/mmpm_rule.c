/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdio.h>
#include <string.h>

#include "mmpm_allocator.h"
#include "mmpm_compile.h"
#include "mmpm_runtime.h"
#include "mmpm_rule.h"
#include "mmpm_rule_internal.h"
#include "mmpm_hash.h"
#include "mmpm_ac.h"
#include "expr/mmpm_expr_internal.h"

static int mmpm_database_is_valid(const mmpm_database_t *db, bool is_add)
{
    if (db == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (db->mmpm_matcher && db->type < MMPM_MATCHER_TYPE_MAX) {
        return MMPM_SUCCESS;
    }

    if (is_add && db->mmpm_matcher == NULL && db->type == MMPM_MATCHER_NONE) {
        return MMPM_SUCCESS;
    }

    return MMPM_INVALID_PARA;
}

static int mmpm_handler_is_valid(mmpm_handler_t *handler, bool is_add)
{
    if (handler == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (handler->mmpm_matcher && handler->type < MMPM_MATCHER_TYPE_MAX) {
        return MMPM_SUCCESS;
    }

    if (is_add && handler->mmpm_matcher == NULL && handler->type == MMPM_MATCHER_NONE) {
        return MMPM_SUCCESS;
    }

    return MMPM_INVALID_PARA;
}

int mmpm_rule_add_direct(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;

    if (mmpm_handler_is_valid(handler, true) || rule == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (handler->mmpm_matcher == NULL && handler->type == MMPM_MATCHER_NONE) {
        printf("first import the rules need choose hash or ac.\n");
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MMPM_MATCHER_TYPE_HASH:
                ret = hash_table_add(handler->mmpm_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                /*
                 * Batch hash insertion keeps the already inserted part on error
                 * and returns the failure to the caller.
                 */
                if (ret) {
                    return ret;
                }
                break;
            case MMPM_MATCHER_TYPE_AC:
                if (handler->is_built) {
                    return MMPM_BUILT_ALREADY;
                }
                ret = ac_automaton_insert(handler->mmpm_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    ac_automaton_destroy(handler->mmpm_matcher);
                    return ret;
                }
                break;
            default:
                break;
        }
    }

    if (handler->type == MMPM_MATCHER_TYPE_AC) {
        ret = ac_automaton_build(handler->mmpm_matcher);
        if (ret) {
            (void)ac_automaton_destroy(handler->mmpm_matcher);
        } else {
            handler->is_built = true;
        }
    }

    return ret;
}

int mmpm_rule_add_database_direct(mmpm_database_t *db, mmpm_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;

    if (mmpm_database_is_valid(db, true) || rule == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (db->mmpm_matcher == NULL && db->type == MMPM_MATCHER_NONE) {
        printf("first import the rules need choose hash or ac.\n");
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        switch (db->type) {
            case MMPM_MATCHER_TYPE_HASH:
                ret = hash_table_add(db->mmpm_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    return ret;
                }
                break;
            case MMPM_MATCHER_TYPE_AC:
                if (db->is_built) {
                    return MMPM_BUILT_ALREADY;
                }
                ret = ac_automaton_insert(db->mmpm_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    ac_automaton_destroy(db->mmpm_matcher);
                    return ret;
                }
                break;
            default:
                break;
        }
    }

    if (db->type == MMPM_MATCHER_TYPE_AC) {
        ret = ac_automaton_build(db->mmpm_matcher);
        if (ret) {
            (void)ac_automaton_destroy(db->mmpm_matcher);
        } else {
            db->is_built = true;
        }
    }

    return ret;
}

int mmpm_rule_find_direct(mmpm_handler_t *handler, mmpm_rule_t *rule)
{
    if (rule == NULL || rule->key == NULL || mmpm_handler_is_valid(handler, false)) {
        return MMPM_INVALID_PARA;
    }

    switch (handler->type) {
        case MMPM_MATCHER_TYPE_HASH:
            return hash_table_find(handler->mmpm_matcher, rule->key, rule->key_len, &(rule->alias));
        case MMPM_MATCHER_TYPE_AC:
            if (!handler->is_built) {
                return MMPM_INVALID_PARA;
            }
            return ac_automaton_match(handler->mmpm_matcher, rule->key, rule->key_len);
        default:
            return MMPM_INVALID_PARA;
    }
}

int mmpm_rule_find_database_direct(const mmpm_database_t *db, mmpm_rule_t *rule)
{
    if (rule == NULL || rule->key == NULL || mmpm_database_is_valid(db, false)) {
        return MMPM_INVALID_PARA;
    }

    switch (db->type) {
        case MMPM_MATCHER_TYPE_HASH:
            return hash_table_find(db->mmpm_matcher, rule->key, rule->key_len, &(rule->alias));
        case MMPM_MATCHER_TYPE_AC:
            if (!db->is_built) {
                return MMPM_INVALID_PARA;
            }
            return ac_automaton_match(db->mmpm_matcher, rule->key, rule->key_len);
        default:
            return MMPM_INVALID_PARA;
    }
}

int mmpm_rule_remove_direct(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = MMPM_SUCCESS;

    if (rule == NULL || rule_num == 0 || mmpm_handler_is_valid(handler, false)) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MMPM_MATCHER_TYPE_HASH:
                ret = hash_table_remove(handler->mmpm_matcher, rule[i].key, rule[i].key_len);
                if (ret != MMPM_SUCCESS) {
                    return ret;
                }
                break;
            case MMPM_MATCHER_TYPE_AC:
                if (!handler->is_built) {
                    return MMPM_INVALID_PARA;
                }
                ret = ac_automaton_remove(handler->mmpm_matcher, rule[i].alias,
                                          rule[i].key, rule[i].key_len);
                if (ret != MMPM_SUCCESS) {
                    return ret;
                }
                break;
            default:
                return MMPM_INVALID_PARA;
        }
    }

    return ret;
}

static bool mmpm_rule_has_key(const mmpm_rule_t *rule)
{
    return rule != NULL && rule->key != NULL && rule->key_len != 0;
}

static bool mmpm_rule_is_combination_remove(const mmpm_rule_t *rule)
{
    return rule != NULL && rule->key == NULL && rule->key_len == 0;
}

static bool mmpm_rule_remove_param_is_valid(const mmpm_rule_t *rule)
{
    return mmpm_rule_has_key(rule) || mmpm_rule_is_combination_remove(rule);
}

static void mmpm_rule_free_exprs(char **exprs, uint32_t count)
{
    uint32_t i;

    if (exprs == NULL) {
        return;
    }

    for (i = 0; i < count; i++) {
        if (exprs[i] != NULL) {
            mmpm_free(exprs[i]);
        }
    }
    mmpm_free(exprs);
}

static int mmpm_rule_to_exprs(mmpm_rule_t *rule,
                              uint32_t rule_num,
                              char ***exprs_out,
                              uint32_t **ids_out)
{
    char **exprs = NULL;
    uint32_t *ids = NULL;
    uint32_t i;

    if (rule == NULL || rule_num == 0 || exprs_out == NULL || ids_out == NULL) {
        return MMPM_INVALID_PARA;
    }

    exprs = (char **)mmpm_calloc(rule_num, sizeof(char *));
    ids = (uint32_t *)mmpm_calloc(rule_num, sizeof(uint32_t));
    if (exprs == NULL || ids == NULL) {
        mmpm_rule_free_exprs(exprs, rule_num);
        if (ids != NULL) {
            mmpm_free(ids);
        }
        return MMPM_MALLOC_FAIL;
    }

    for (i = 0; i < rule_num; i++) {
        if (rule[i].key == NULL || rule[i].key_len == 0 ||
            memchr(rule[i].key, '\0', rule[i].key_len) != NULL) {
            mmpm_rule_free_exprs(exprs, rule_num);
            mmpm_free(ids);
            return MMPM_INVALID_PARA;
        }

        exprs[i] = (char *)mmpm_calloc(rule[i].key_len + 1, sizeof(char));
        if (exprs[i] == NULL) {
            mmpm_rule_free_exprs(exprs, rule_num);
            mmpm_free(ids);
            return MMPM_MALLOC_FAIL;
        }
        (void)memcpy(exprs[i], rule[i].key, rule[i].key_len);
        ids[i] = rule[i].alias;
    }

    *exprs_out = exprs;
    *ids_out = ids;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_add_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num)
{
    char **exprs = NULL;
    uint32_t *ids = NULL;
    int ret;

    if (handler == NULL) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_rule_to_exprs(rule, rule_num, &exprs, &ids);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = mmpm_compile_multi_ext(handler, (const char *const *)exprs, NULL, ids, NULL, rule_num);

    mmpm_rule_free_exprs(exprs, rule_num);
    mmpm_free(ids);
    return ret;
}

__attribute__((visibility("default")))
int mmpm_find_rule(mmpm_handler_t *handler, mmpm_rule_t *rule)
{
    mmpm_compile_context_t *ctx;
    uint32_t *ids = NULL;
    uint32_t id_count = 0;
    uint32_t id_capacity;
    int ret = MMPM_SUCCESS;

    if (rule == NULL || rule->key == NULL || mmpm_handler_is_valid(handler, false)) {
        return MMPM_INVALID_PARA;
    }

    switch (handler->type) {
        case MMPM_MATCHER_TYPE_HASH:
            ret = mmpm_rule_find_direct(handler, rule);
            break;
        case MMPM_MATCHER_TYPE_AC:
            if (!handler->is_built) {
                return MMPM_INVALID_PARA;
            }
            ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
            if (ctx == NULL) {
                return MMPM_INVALID_PARA;
            }

            id_capacity = ctx->expr_count + ctx->comb_count;
            if (id_capacity != 0) {
                ids = (uint32_t *)mmpm_calloc(id_capacity, sizeof(uint32_t));
                if (ids == NULL) {
                    return MMPM_MALLOC_FAIL;
                }
            }

            ret = mmpm_scan_expr(handler, rule->key, rule->key_len, ids, &id_count, id_capacity);
            if (ret == MMPM_SUCCESS && id_count != 0) {
                rule->alias = ids[0];
            }
            if (ids != NULL) {
                mmpm_free(ids);
            }
            break;
        default:
            break;
    }

    return ret;
}

__attribute__((visibility("default")))
int mmpm_remove_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;
    bool removed_from_ctx = false;

    if (rule == NULL || rule_num == 0 || mmpm_handler_is_valid(handler, false)) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        if (!mmpm_rule_remove_param_is_valid(&rule[i])) {
            return MMPM_INVALID_PARA;
        }
    }

    for (i = 0; i < rule_num; i++) {
        bool has_key = mmpm_rule_has_key(&rule[i]);

        if (has_key) {
            ret = mmpm_rule_remove_direct(handler, &rule[i], 1);
            if (ret != MMPM_SUCCESS) {
                goto end;
            }
        }

        if (has_key) {
            ret = mmpm_compile_context_remove_id((mmpm_compile_context_t *)handler->mmpm_compile_ctx, rule[i].alias);
        } else {
            ret = mmpm_compile_context_remove_comb_id((mmpm_compile_context_t *)handler->mmpm_compile_ctx,
                                                      rule[i].alias);
        }
        if (ret == MMPM_SUCCESS) {
            removed_from_ctx = true;
            continue;
        }
        if (!has_key || ret != MMPM_FIND_FAIL) {
            goto end;
        }
        ret = MMPM_SUCCESS;
    }

    if (removed_from_ctx && handler->type == MMPM_MATCHER_TYPE_AC && handler->mmpm_matcher != NULL) {
        ((ACAutomaton *)handler->mmpm_matcher)->generation++;
    }

end:
    return ret;
}

__attribute__((visibility("default")))
int mmpm_set_hash_func(mmpm_handler_t *handler, hash_calc_func hash_func)
{
    if (handler == NULL || handler->mmpm_matcher == NULL ||
        handler->type != MMPM_MATCHER_TYPE_HASH || hash_func == NULL) {
        return MMPM_INVALID_PARA;
    }

    hash_table_set_hash_func(handler->mmpm_matcher, hash_func);
    return MMPM_SUCCESS;
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdio.h>

#include "mmpm_rule.h"
#include "mmpm_hash.h"
#include "mmpm_ac.h"

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

__attribute__((visibility("default")))
int mmpm_add_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num)
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

__attribute__((visibility("default")))
int mmpm_find_rule(mmpm_handler_t *handler, mmpm_rule_t *rule)
{
    int ret = MMPM_SUCCESS;

    if (rule == NULL || rule->key == NULL || mmpm_handler_is_valid(handler, false)) {
        return MMPM_INVALID_PARA;
    }

    switch (handler->type) {
        case MMPM_MATCHER_TYPE_HASH:
            ret = hash_table_find(handler->mmpm_matcher, rule->key, rule->key_len, &(rule->alias));
            break;
        case MMPM_MATCHER_TYPE_AC:
            if (!handler->is_built) {
                return MMPM_INVALID_PARA;
            }
            ret = ac_automaton_match(handler->mmpm_matcher, rule->key, rule->key_len);
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

    if (rule == NULL || mmpm_handler_is_valid(handler, false)) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MMPM_MATCHER_TYPE_HASH:
                ret = hash_table_remove(handler->mmpm_matcher, rule[i].key, rule[i].key_len);
                if (ret) {
                    goto end;
                }
                break;
            case MMPM_MATCHER_TYPE_AC:
                if (!handler->is_built) {
                    ret = MMPM_INVALID_PARA;
                    goto end;
                }
                ret = ac_automaton_remove(handler->mmpm_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    goto end;
                }
                break;
            default:
                break;
        }
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

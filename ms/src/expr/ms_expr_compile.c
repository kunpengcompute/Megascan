/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ms.h"
#include "ms_allocator.h"
#include "ms_compile.h"
#include "ms_expr.h"
#include "ms_expr_internal.h"
#include "ms_logical.h"
#include "../ms_database_internal.h"
#include "../ms_rule_internal.h"

static int ms_compile_compare_u32(const void *left, const void *right)
{
    uint32_t left_value = *(const uint32_t *)left;
    uint32_t right_value = *(const uint32_t *)right;

    return (left_value > right_value) - (left_value < right_value);
}

static int ms_compile_validate_unique_ids(const uint32_t *ids,
                                          uint32_t elements)
{
    uint32_t *sorted_ids;
    uint32_t i;

    if (ids == NULL) {
        return MS_SUCCESS;
    }

    if (elements != 0 && SIZE_MAX / sizeof(uint32_t) < elements) {
        return MS_MALLOC_FAIL;
    }
    sorted_ids = (uint32_t *)ms_malloc((size_t)elements * sizeof(uint32_t));
    if (sorted_ids == NULL) {
        return MS_MALLOC_FAIL;
    }

    (void)memcpy(sorted_ids, ids, (size_t)elements * sizeof(uint32_t));
    qsort(sorted_ids, elements, sizeof(uint32_t), ms_compile_compare_u32);
    for (i = 1; i < elements; i++) {
        if (sorted_ids[i - 1] == sorted_ids[i]) {
            ms_free(sorted_ids);
            return MS_INVALID_PARA;
        }
    }

    ms_free(sorted_ids);
    return MS_SUCCESS;
}

static int ms_compile_validate(const char *const *expressions, const uint32_t *flags,
                                 const uint32_t *ids, const ms_expr_ext_t *ext,
                                 uint32_t elements)
{
    uint32_t i;
    int ret;

    if (expressions == NULL || elements == 0) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < elements; i++) {
        uint32_t flag = flags ? flags[i] : MS_FLAG_NONE;

        if (expressions[i] == NULL || expressions[i][0] == '\0') {
            return MS_INVALID_PARA;
        }
        if (flag & ~MS_EXPR_SUPPORTED_FLAGS) {
            return MS_INVALID_PARA;
        }
        if ((flag & MS_FLAG_COMBINATION) && ids == NULL) {
            return MS_INVALID_PARA;
        }
        if ((flag & MS_FLAG_SINGLEMATCH) && ((flag & MS_FLAG_COMBINATION) == 0)) {
            return MS_INVALID_PARA;
        }
        if (ext != NULL) {
            if (ext[i].flags & ~(MS_EXT_FLAG_MIN_OFFSET | MS_EXT_FLAG_MAX_OFFSET)) {
                return MS_INVALID_PARA;
            }
            if (ext[i].flags != 0 && ((flag & MS_FLAG_COMBINATION) == 0)) {
                return MS_INVALID_PARA;
            }
            if ((ext[i].flags & (MS_EXT_FLAG_MIN_OFFSET | MS_EXT_FLAG_MAX_OFFSET)) ==
                    (MS_EXT_FLAG_MIN_OFFSET | MS_EXT_FLAG_MAX_OFFSET) &&
                ext[i].min_offset > ext[i].max_offset) {
                return MS_INVALID_PARA;
            }
        }
    }

    ret = ms_compile_validate_unique_ids(ids, elements);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    return MS_SUCCESS;
}

bool ms_compile_is_combination(const uint32_t *flags, uint32_t index)
{
    return flags && (flags[index] & MS_FLAG_COMBINATION);
}

static int ms_compile_find_lkey(const ms_compile_context_t *new_ctx,
                                  const ms_compile_context_t *old_ctx,
                                  uint32_t id,
                                  uint32_t *lkey)
{
    int ret = ms_compile_context_find_lkey(new_ctx, id, lkey);
    if (ret == MS_SUCCESS) {
        return ret;
    }

    return ms_compile_context_find_lkey(old_ctx, id, lkey);
}

static int ms_compile_remap_operand(const ms_compile_context_t *new_ctx,
                                      const ms_compile_context_t *old_ctx,
                                      uint32_t *operand)
{
    uint32_t lkey;
    int ret;

    if ((*operand & MS_LOGICAL_OP_REF) != 0) {
        return MS_SUCCESS;
    }

    ret = ms_compile_find_lkey(new_ctx, old_ctx, *operand, &lkey);
    if (ret != MS_SUCCESS) {
        return MS_INVALID_PARA;
    }

    *operand = lkey;
    return MS_SUCCESS;
}

static int ms_compile_remap_logical_refs(const ms_compile_context_t *new_ctx,
                                           const ms_compile_context_t *old_ctx,
                                           ms_logical_tree_t *tree)
{
    uint32_t i;
    int ret;

    for (i = 0; i < tree->op_count; i++) {
        if (tree->ops[i].op != MS_LOGICAL_OP_NOT) {
            ret = ms_compile_remap_operand(new_ctx, old_ctx, &tree->ops[i].left);
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }

        ret = ms_compile_remap_operand(new_ctx, old_ctx, &tree->ops[i].right);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    ret = ms_compile_remap_operand(new_ctx, old_ctx, &tree->result);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    return MS_SUCCESS;
}

static int ms_compile_operand_max_lkey(uint32_t operand, uint32_t *max_lkey)
{
    if ((operand & MS_LOGICAL_OP_REF) != 0) {
        return MS_SUCCESS;
    }

    if (max_lkey == NULL) {
        return MS_INVALID_PARA;
    }

    if (operand > *max_lkey) {
        *max_lkey = operand;
    }
    return MS_SUCCESS;
}

static int ms_compile_find_tree_max_lkey(const ms_logical_tree_t *tree,
                                           uint32_t *max_lkey)
{
    uint32_t i;
    int ret;

    if (tree == NULL || max_lkey == NULL) {
        return MS_INVALID_PARA;
    }

    *max_lkey = 0;
    for (i = 0; i < tree->op_count; i++) {
        if (tree->ops[i].op != MS_LOGICAL_OP_NOT) {
            ret = ms_compile_operand_max_lkey(tree->ops[i].left, max_lkey);
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }
        ret = ms_compile_operand_max_lkey(tree->ops[i].right, max_lkey);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    return ms_compile_operand_max_lkey(tree->result, max_lkey);
}

static int ms_compile_final_lkey_for_operand(uint32_t op_start,
                                               uint32_t op_count,
                                               uint32_t *operand)
{
    uint32_t index;

    if (operand == NULL) {
        return MS_INVALID_PARA;
    }
    if ((*operand & MS_LOGICAL_OP_REF) == 0) {
        return MS_SUCCESS;
    }

    index = *operand & ~MS_LOGICAL_OP_REF;
    if (index >= op_count) {
        return MS_INVALID_PARA;
    }

    *operand = op_start + index;
    return MS_SUCCESS;
}

static int ms_compile_assign_logical_op_lkeys(const ms_compile_context_t *new_ctx,
                                                const ms_compile_context_t *old_ctx,
                                                ms_logical_tree_t *tree)
{
    uint32_t i;
    uint32_t max_lkey = 0;
    uint32_t op_start;
    int ret;

    if (tree == NULL || tree->op_count == 0) {
        return MS_INVALID_PARA;
    }

    ret = ms_compile_find_tree_max_lkey(tree, &max_lkey);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    op_start = max_lkey + 1;
    if (new_ctx != NULL && new_ctx->logical_count > op_start) {
        op_start = new_ctx->logical_count;
    }
    if (old_ctx != NULL && old_ctx->logical_count > op_start) {
        op_start = old_ctx->logical_count;
    }

    for (i = 0; i < tree->op_count; i++) {
        tree->ops[i].id = op_start + i;

        if (tree->ops[i].op != MS_LOGICAL_OP_NOT) {
            ret = ms_compile_final_lkey_for_operand(op_start, tree->op_count,
                                                      &tree->ops[i].left);
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }

        ret = ms_compile_final_lkey_for_operand(op_start, tree->op_count,
                                                  &tree->ops[i].right);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    return ms_compile_final_lkey_for_operand(op_start, tree->op_count, &tree->result);
}

static int ms_compile_collect_exprs(ms_compile_context_t *new_ctx,
                                      const ms_compile_context_t *old_ctx,
                                      const uint32_t *flags,
                                      const uint32_t *ids,
                                      uint32_t elements)
{
    uint32_t i;
    uint32_t lkey = old_ctx ? old_ctx->logical_count : 0;
    int ret;

    for (i = 0; i < elements; i++) {
        uint32_t id = ids ? ids[i] : 0;
        uint32_t flag = flags ? flags[i] : MS_FLAG_NONE;

        if (ms_compile_context_has_id(old_ctx, id)) {
            return MS_INVALID_PARA;
        }

        if (ms_compile_is_combination(flags, i)) {
            continue;
        }

        ret = ms_compile_context_add_expr(new_ctx, id, flag, lkey);
        if (ret != MS_SUCCESS) {
            return ret;
        }
        lkey++;
    }

    return MS_SUCCESS;
}

static int ms_compile_parse_combinations(ms_compile_context_t *ctx,
                                           const ms_compile_context_t *old_ctx,
                                           const char *const *expressions,
                                           const uint32_t *flags,
                                           const uint32_t *ids,
                                           const ms_expr_ext_t *ext,
                                           uint32_t elements)
{
    uint32_t i;
    int ret;

    for (i = 0; i < elements; i++) {
        ms_logical_tree_t tree;

        if (!ms_compile_is_combination(flags, i)) {
            continue;
        }
        if (flags && ((flags[i] & MS_FLAG_QUIET) != 0)) {
            continue;
        }

        if (ms_compile_context_has_id(old_ctx, ids ? ids[i] : 0)) {
            return MS_INVALID_PARA;
        }

        ret = ms_logical_parse(expressions[i], &tree);
        if (ret != MS_SUCCESS) {
            return ret;
        }

        ret = ms_compile_remap_logical_refs(ctx, old_ctx, &tree);
        if (ret != MS_SUCCESS) {
            ms_logical_tree_destroy(&tree);
            return ret;
        }

        ret = ms_compile_assign_logical_op_lkeys(ctx, old_ctx, &tree);
        if (ret != MS_SUCCESS) {
            ms_logical_tree_destroy(&tree);
            return ret;
        }

        ret = ms_compile_context_add_comb(ctx, ids ? ids[i] : 0,
                                            flags ? flags[i] : MS_FLAG_NONE,
                                            ext ? &ext[i] : NULL,
                                            &tree);
        if (ret != MS_SUCCESS) {
            ms_logical_tree_destroy(&tree);
            return ret;
        }
    }

    return MS_SUCCESS;
}

static int ms_compile_add_plain_rules_handler(ms_handler_t *handler,
                                                const char *const *expressions,
                                                const uint32_t *flags,
                                                const uint32_t *ids,
                                                uint32_t elements)
{
    uint32_t i;
    uint32_t plain_count = 0;
    ms_rule_t *rules = NULL;
    int ret;

    rules = (ms_rule_t *)ms_calloc(elements, sizeof(ms_rule_t));
    if (rules == NULL) {
        return MS_MALLOC_FAIL;
    }

    for (i = 0; i < elements; i++) {
        size_t len = strlen(expressions[i]);
        if (len == 0 || len > UINT32_MAX) {
            ms_free(rules);
            return MS_INVALID_PARA;
        }

        if (ms_compile_is_combination(flags, i)) {
            continue;
        }

        rules[plain_count].alias = ids ? ids[i] : 0;
        rules[plain_count].key_len = (uint32_t)len;
        rules[plain_count].key = (uint8_t *)expressions[i];
        plain_count++;
    }

    ret = plain_count ? ms_rule_add_direct(handler, rules, plain_count) : MS_SUCCESS;
    ms_free(rules);
    return ret;
}

static int ms_compile_add_plain_rules_database(ms_database_t *db,
                                                 const ms_compile_context_t *ctx,
                                                 const char *const *expressions,
                                                 const uint32_t *flags,
                                                 const uint32_t *ids,
                                        uint32_t elements)
{
    uint32_t i;
    uint32_t plain_count = 0;
    ms_rule_t *rules = NULL;
    int ret;

    rules = (ms_rule_t *)ms_calloc(elements, sizeof(ms_rule_t));
    if (rules == NULL) {
        return MS_MALLOC_FAIL;
    }

    for (i = 0; i < elements; i++) {
        size_t len = strlen(expressions[i]);
        if (len == 0 || len > UINT32_MAX) {
            ms_free(rules);
            return MS_INVALID_PARA;
        }

        if (ms_compile_is_combination(flags, i)) {
            continue;
        }

        if (ctx != NULL && ctx->ac_aliases_are_lkeys) {
            if (plain_count >= ctx->expr_count) {
                ms_free(rules);
                return MS_INVALID_PARA;
            }
            rules[plain_count].alias = ctx->exprs[plain_count].lkey;
        } else {
            rules[plain_count].alias = ids ? ids[i] : 0;
        }
        rules[plain_count].key_len = (uint32_t)len;
        rules[plain_count].key = (uint8_t *)expressions[i];
        plain_count++;
    }

    ret = plain_count ? ms_rule_add_database_direct(db, rules, plain_count) : MS_SUCCESS;
    ms_free(rules);
    return ret;
}

__attribute__((visibility("default")))
int ms_compile_multi(ms_handler_t *handler, const char *const *expressions, const uint32_t *flags,
                       const uint32_t *ids, uint32_t elements)
{
    return ms_compile_multi_ext(handler, expressions, flags, ids, NULL, elements);
}

__attribute__((visibility("default")))
int ms_compile_multi_ext(ms_handler_t *handler,
                           const char *const *expressions,
                           const uint32_t *flags,
                           const uint32_t *ids,
                           const ms_expr_ext_t *ext,
                           uint32_t elements)
{
    int ret;
    ms_compile_context_t *new_ctx = NULL;
    ms_compile_context_t *old_ctx = NULL;

    if (handler == NULL) {
        return MS_INVALID_PARA;
    }

    ret = ms_compile_validate(expressions, flags, ids, ext, elements);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    new_ctx = (ms_compile_context_t *)ms_calloc(1, sizeof(ms_compile_context_t));
    if (new_ctx == NULL) {
        return MS_MALLOC_FAIL;
    }
    old_ctx = (ms_compile_context_t *)handler->ms_compile_ctx;
    ret = ms_compile_collect_exprs(new_ctx, old_ctx, flags, ids, elements);
    if (ret != MS_SUCCESS) {
        ms_compile_context_free(new_ctx);
        return ret;
    }

    ret = ms_compile_parse_combinations(new_ctx, old_ctx, expressions, flags, ids, ext, elements);
    if (ret != MS_SUCCESS) {
        ms_compile_context_free(new_ctx);
        return ret;
    }

    ret = ms_compile_add_plain_rules_handler(handler, expressions, flags, ids, elements);
    if (ret != MS_SUCCESS) {
        ms_compile_context_free(new_ctx);
        return ret;
    }

    ret = ms_compile_append_context(handler, new_ctx);
    if (ret != MS_SUCCESS) {
        ms_compile_context_free(new_ctx);
        return ret;
    }

    if (handler->ms_compile_ctx != new_ctx) {
        ms_compile_context_free(new_ctx);
    }
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_add_exprs(ms_handler_t *handler, const char *const *expr, const uint32_t *ids,
                   const uint32_t *flags, uint32_t elements)
{
    return ms_compile_multi(handler, expr, flags, ids, elements);
}

int ms_compile_database_multi_native(ms_database_t *db,
                                       const char *const *expressions,
                                       const uint32_t *flags,
                                       const uint32_t *ids,
                                       const ms_expr_ext_t *ext,
                                       uint32_t elements)
{
    int ret;
    ms_compile_context_t *new_ctx = NULL;
    ms_compile_context_t *old_ctx = NULL;

    if (db == NULL) {
        return MS_INVALID_PARA;
    }

    ret = ms_compile_validate(expressions, flags, ids, ext, elements);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    new_ctx = (ms_compile_context_t *)ms_calloc(1, sizeof(ms_compile_context_t));
    if (new_ctx == NULL) {
        return MS_MALLOC_FAIL;
    }
    new_ctx->ac_aliases_are_lkeys = (db->type == MS_MATCHER_TYPE_AC);

    old_ctx = (ms_compile_context_t *)db->ms_compile_ctx;
    ret = ms_compile_collect_exprs(new_ctx, old_ctx, flags, ids, elements);
    if (ret != MS_SUCCESS) {
        ms_compile_context_free(new_ctx);
        return ret;
    }

    ret = ms_compile_parse_combinations(new_ctx, old_ctx, expressions, flags, ids, ext, elements);
    if (ret != MS_SUCCESS) {
        ms_compile_context_free(new_ctx);
        return ret;
    }

    ret = ms_compile_add_plain_rules_database(db, new_ctx, expressions, flags,
                                                ids, elements);
    if (ret != MS_SUCCESS) {
        ms_compile_context_free(new_ctx);
        return ret;
    }

    ret = ms_compile_append_database_context(db, new_ctx);
    if (ret != MS_SUCCESS) {
        ms_compile_context_free(new_ctx);
        return ret;
    }

    if (db->ms_compile_ctx != new_ctx) {
        ms_compile_context_free(new_ctx);
    }
    return MS_SUCCESS;
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mmpm.h"
#include "mmpm_allocator.h"
#include "mmpm_expr.h"
#include "mmpm_expr_internal.h"
#include "mmpm_logical.h"

static int mmpm_compile_validate(const char *const *expressions, const uint32_t *flags,
                                 const uint32_t *ids, const mmpm_expr_ext_t *ext,
                                 uint32_t elements)
{
    uint32_t i;

    if (expressions == NULL || elements == 0) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < elements; i++) {
        uint32_t flag = flags ? flags[i] : MMPM_FLAG_NONE;

        if (expressions[i] == NULL || expressions[i][0] == '\0') {
            return MMPM_INVALID_PARA;
        }
        if (flag & ~MMPM_EXPR_SUPPORTED_FLAGS) {
            return MMPM_INVALID_PARA;
        }
        if ((flag & MMPM_FLAG_COMBINATION) && ids == NULL) {
            return MMPM_INVALID_PARA;
        }
        if ((flag & MMPM_FLAG_SINGLEMATCH) && ((flag & MMPM_FLAG_COMBINATION) == 0)) {
            return MMPM_INVALID_PARA;
        }
        if (ext != NULL) {
            if (ext[i].flags & ~(MMPM_EXT_FLAG_MIN_OFFSET | MMPM_EXT_FLAG_MAX_OFFSET)) {
                return MMPM_INVALID_PARA;
            }
            if (ext[i].flags != 0 && ((flag & MMPM_FLAG_COMBINATION) == 0)) {
                return MMPM_INVALID_PARA;
            }
            if ((ext[i].flags & (MMPM_EXT_FLAG_MIN_OFFSET | MMPM_EXT_FLAG_MAX_OFFSET)) ==
                    (MMPM_EXT_FLAG_MIN_OFFSET | MMPM_EXT_FLAG_MAX_OFFSET) &&
                ext[i].min_offset > ext[i].max_offset) {
                return MMPM_INVALID_PARA;
            }
        }
        if (ids != NULL) {
            uint32_t j;

            for (j = 0; j < i; j++) {
                if (ids[j] == ids[i]) {
                    return MMPM_INVALID_PARA;
                }
            }
        }
    }

    return MMPM_SUCCESS;
}

bool mmpm_compile_is_combination(const uint32_t *flags, uint32_t index)
{
    return flags && (flags[index] & MMPM_FLAG_COMBINATION);
}

static int mmpm_compile_find_lkey(const mmpm_compile_context_t *new_ctx,
                                  const mmpm_compile_context_t *old_ctx,
                                  uint32_t id,
                                  uint32_t *lkey)
{
    int ret = mmpm_compile_context_find_lkey(new_ctx, id, lkey);
    if (ret == MMPM_SUCCESS) {
        return ret;
    }

    return mmpm_compile_context_find_lkey(old_ctx, id, lkey);
}

static int mmpm_compile_remap_operand(const mmpm_compile_context_t *new_ctx,
                                      const mmpm_compile_context_t *old_ctx,
                                      uint32_t *operand)
{
    uint32_t lkey;
    int ret;

    if ((*operand & MMPM_LOGICAL_OP_REF) != 0) {
        return MMPM_SUCCESS;
    }

    ret = mmpm_compile_find_lkey(new_ctx, old_ctx, *operand, &lkey);
    if (ret != MMPM_SUCCESS) {
        return MMPM_INVALID_PARA;
    }

    *operand = lkey;
    return MMPM_SUCCESS;
}

static int mmpm_compile_remap_logical_refs(const mmpm_compile_context_t *new_ctx,
                                           const mmpm_compile_context_t *old_ctx,
                                           mmpm_logical_tree_t *tree)
{
    uint32_t i;
    int ret;

    for (i = 0; i < tree->op_count; i++) {
        if (tree->ops[i].op != MMPM_LOGICAL_OP_NOT) {
            ret = mmpm_compile_remap_operand(new_ctx, old_ctx, &tree->ops[i].left);
            if (ret != MMPM_SUCCESS) {
                return ret;
            }
        }

        ret = mmpm_compile_remap_operand(new_ctx, old_ctx, &tree->ops[i].right);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    ret = mmpm_compile_remap_operand(new_ctx, old_ctx, &tree->result);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_operand_max_lkey(uint32_t operand, uint32_t *max_lkey)
{
    if ((operand & MMPM_LOGICAL_OP_REF) != 0) {
        return MMPM_SUCCESS;
    }

    if (max_lkey == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (operand > *max_lkey) {
        *max_lkey = operand;
    }
    return MMPM_SUCCESS;
}

static int mmpm_compile_find_tree_max_lkey(const mmpm_logical_tree_t *tree,
                                           uint32_t *max_lkey)
{
    uint32_t i;
    int ret;

    if (tree == NULL || max_lkey == NULL) {
        return MMPM_INVALID_PARA;
    }

    *max_lkey = 0;
    for (i = 0; i < tree->op_count; i++) {
        if (tree->ops[i].op != MMPM_LOGICAL_OP_NOT) {
            ret = mmpm_compile_operand_max_lkey(tree->ops[i].left, max_lkey);
            if (ret != MMPM_SUCCESS) {
                return ret;
            }
        }
        ret = mmpm_compile_operand_max_lkey(tree->ops[i].right, max_lkey);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    return mmpm_compile_operand_max_lkey(tree->result, max_lkey);
}

static int mmpm_compile_final_lkey_for_operand(uint32_t op_start,
                                               uint32_t op_count,
                                               uint32_t *operand)
{
    uint32_t index;

    if (operand == NULL) {
        return MMPM_INVALID_PARA;
    }
    if ((*operand & MMPM_LOGICAL_OP_REF) == 0) {
        return MMPM_SUCCESS;
    }

    index = *operand & ~MMPM_LOGICAL_OP_REF;
    if (index >= op_count) {
        return MMPM_INVALID_PARA;
    }

    *operand = op_start + index;
    return MMPM_SUCCESS;
}

static int mmpm_compile_assign_logical_op_lkeys(const mmpm_compile_context_t *new_ctx,
                                                const mmpm_compile_context_t *old_ctx,
                                                mmpm_logical_tree_t *tree)
{
    uint32_t i;
    uint32_t max_lkey = 0;
    uint32_t op_start;
    int ret;

    if (tree == NULL || tree->op_count == 0) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_compile_find_tree_max_lkey(tree, &max_lkey);
    if (ret != MMPM_SUCCESS) {
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

        if (tree->ops[i].op != MMPM_LOGICAL_OP_NOT) {
            ret = mmpm_compile_final_lkey_for_operand(op_start, tree->op_count,
                                                      &tree->ops[i].left);
            if (ret != MMPM_SUCCESS) {
                return ret;
            }
        }

        ret = mmpm_compile_final_lkey_for_operand(op_start, tree->op_count,
                                                  &tree->ops[i].right);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    return mmpm_compile_final_lkey_for_operand(op_start, tree->op_count, &tree->result);
}

static int mmpm_compile_collect_exprs(mmpm_compile_context_t *new_ctx,
                                      const mmpm_compile_context_t *old_ctx,
                                      const uint32_t *flags,
                                      const uint32_t *ids,
                                      uint32_t elements)
{
    uint32_t i;
    uint32_t lkey = old_ctx ? old_ctx->logical_count : 0;
    int ret;

    for (i = 0; i < elements; i++) {
        uint32_t id = ids ? ids[i] : 0;
        uint32_t flag = flags ? flags[i] : MMPM_FLAG_NONE;

        if (mmpm_compile_context_has_id(old_ctx, id)) {
            return MMPM_INVALID_PARA;
        }

        if (mmpm_compile_is_combination(flags, i)) {
            continue;
        }

        ret = mmpm_compile_context_add_expr(new_ctx, id, flag, lkey);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
        lkey++;
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_parse_combinations(mmpm_compile_context_t *ctx,
                                           const mmpm_compile_context_t *old_ctx,
                                           const char *const *expressions,
                                           const uint32_t *flags,
                                           const uint32_t *ids,
                                           const mmpm_expr_ext_t *ext,
                                           uint32_t elements)
{
    uint32_t i;
    int ret;

    for (i = 0; i < elements; i++) {
        mmpm_logical_tree_t tree;

        if (!mmpm_compile_is_combination(flags, i)) {
            continue;
        }
        if (flags && ((flags[i] & MMPM_FLAG_QUIET) != 0)) {
            continue;
        }

        if (mmpm_compile_context_has_id(old_ctx, ids ? ids[i] : 0)) {
            return MMPM_INVALID_PARA;
        }

        ret = mmpm_logical_parse(expressions[i], &tree);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }

        ret = mmpm_compile_remap_logical_refs(ctx, old_ctx, &tree);
        if (ret != MMPM_SUCCESS) {
            mmpm_logical_tree_destroy(&tree);
            return ret;
        }

        ret = mmpm_compile_assign_logical_op_lkeys(ctx, old_ctx, &tree);
        if (ret != MMPM_SUCCESS) {
            mmpm_logical_tree_destroy(&tree);
            return ret;
        }

        ret = mmpm_compile_context_add_comb(ctx, ids ? ids[i] : 0,
                                            flags ? flags[i] : MMPM_FLAG_NONE,
                                            ext ? &ext[i] : NULL,
                                            &tree);
        if (ret != MMPM_SUCCESS) {
            mmpm_logical_tree_destroy(&tree);
            return ret;
        }
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_add_plain_rules(mmpm_handler_t *handler,
                                        const char *const *expressions,
                                        const uint32_t *flags,
                                        const uint32_t *ids,
                                        uint32_t elements)
{
    uint32_t i;
    uint32_t plain_count = 0;
    mmpm_rule_t *rules = NULL;
    int ret;

    rules = (mmpm_rule_t *)mmpm_calloc(elements, sizeof(mmpm_rule_t));
    if (rules == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    for (i = 0; i < elements; i++) {
        size_t len = strlen(expressions[i]);
        if (len == 0 || len > UINT32_MAX) {
            mmpm_free(rules);
            return MMPM_INVALID_PARA;
        }

        if (mmpm_compile_is_combination(flags, i)) {
            continue;
        }

        rules[plain_count].alias = ids ? ids[i] : 0;
        rules[plain_count].key_len = (uint32_t)len;
        rules[plain_count].key = (uint8_t *)expressions[i];
        plain_count++;
    }

    ret = plain_count ? mmpm_add_rule(handler, rules, plain_count) : MMPM_SUCCESS;
    mmpm_free(rules);
    return ret;
}

__attribute__((visibility("default")))
int mmpm_compile_multi(mmpm_handler_t *handler, const char *const *expressions, const uint32_t *flags,
                       const uint32_t *ids, uint32_t elements)
{
    return mmpm_compile_multi_ext(handler, expressions, flags, ids, NULL, elements);
}

__attribute__((visibility("default")))
int mmpm_compile_multi_ext(mmpm_handler_t *handler,
                           const char *const *expressions,
                           const uint32_t *flags,
                           const uint32_t *ids,
                           const mmpm_expr_ext_t *ext,
                           uint32_t elements)
{
    int ret;
    mmpm_compile_context_t *new_ctx = NULL;
    mmpm_compile_context_t *old_ctx = NULL;

    if (handler == NULL) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_compile_validate(expressions, flags, ids, ext, elements);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    new_ctx = (mmpm_compile_context_t *)mmpm_calloc(1, sizeof(mmpm_compile_context_t));
    if (new_ctx == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    old_ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
    ret = mmpm_compile_collect_exprs(new_ctx, old_ctx, flags, ids, elements);
    if (ret != MMPM_SUCCESS) {
        mmpm_compile_context_free(new_ctx);
        return ret;
    }

    ret = mmpm_compile_parse_combinations(new_ctx, old_ctx, expressions, flags, ids, ext, elements);
    if (ret != MMPM_SUCCESS) {
        mmpm_compile_context_free(new_ctx);
        return ret;
    }

    ret = mmpm_compile_add_plain_rules(handler, expressions, flags, ids, elements);
    if (ret != MMPM_SUCCESS) {
        mmpm_compile_context_free(new_ctx);
        return ret;
    }

    ret = mmpm_compile_append_context(handler, new_ctx);
    if (ret != MMPM_SUCCESS) {
        mmpm_compile_context_free(new_ctx);
        return ret;
    }

    if (handler->mmpm_compile_ctx != new_ctx) {
        mmpm_compile_context_free(new_ctx);
    }
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_add_exprs(mmpm_handler_t *handler, const char *const *expr, const uint32_t *ids,
                   const uint32_t *flags, uint32_t elements)
{
    return mmpm_compile_multi(handler, expr, flags, ids, elements);
}

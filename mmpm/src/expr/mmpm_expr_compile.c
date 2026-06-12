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
                                 const uint32_t *ids, uint32_t elements)
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
        if ((flag & MMPM_FLAG_COMBINATION) && (flag & MMPM_FLAG_QUIET)) {
            return MMPM_INVALID_PARA;
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

bool mmpm_compile_context_has_id(const mmpm_compile_context_t *ctx, uint32_t id)
{
    uint32_t i;

    if (ctx == NULL) {
        return false;
    }

    for (i = 0; i < ctx->expr_count; i++) {
        if (ctx->exprs[i].id == id) {
            return true;
        }
    }

    for (i = 0; i < ctx->comb_count; i++) {
        if (ctx->combs[i].id == id) {
            return true;
        }
    }

    return false;
}

int mmpm_compile_context_find_lkey(const mmpm_compile_context_t *ctx,
                                   uint32_t id,
                                   uint32_t *lkey)
{
    uint32_t i;

    if (ctx == NULL || lkey == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < ctx->expr_count; i++) {
        if (ctx->exprs[i].id == id) {
            *lkey = ctx->exprs[i].lkey;
            return MMPM_SUCCESS;
        }
    }

    return MMPM_FIND_FAIL;
}

const mmpm_compile_expr_t *mmpm_compile_context_find_expr(const mmpm_compile_context_t *ctx,
                                                          uint32_t id)
{
    uint32_t i;

    if (ctx == NULL) {
        return NULL;
    }

    for (i = 0; i < ctx->expr_count; i++) {
        if (ctx->exprs[i].id == id) {
            return &ctx->exprs[i];
        }
    }

    return NULL;
}

uint32_t mmpm_compile_logical_byte_count(uint32_t logical_count)
{
    return (logical_count + 8u - 1) / 8u;
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

void mmpm_compile_context_free(mmpm_compile_context_t *ctx)
{
    uint32_t i;

    if (ctx == NULL) {
        return;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        mmpm_logical_tree_destroy(&ctx->combs[i].tree);
    }

    if (ctx->exprs != NULL) {
        mmpm_free(ctx->exprs);
    }

    if (ctx->combs != NULL) {
        mmpm_free(ctx->combs);
    }

    mmpm_free(ctx);
}

void mmpm_compile_context_destroy(mmpm_handler_t *handler)
{
    if (handler == NULL || handler->mmpm_compile_ctx == NULL) {
        return;
    }

    mmpm_compile_context_free((mmpm_compile_context_t *)handler->mmpm_compile_ctx);
    handler->mmpm_compile_ctx = NULL;
}

static int mmpm_compile_context_reserve_exprs(mmpm_compile_context_t *ctx)
{
    mmpm_compile_expr_t *exprs = NULL;
    uint32_t new_capacity;

    if (ctx->expr_count < ctx->expr_capacity) {
        return MMPM_SUCCESS;
    }

    new_capacity = ctx->expr_capacity ? ctx->expr_capacity * 2 : 4;
    exprs = (mmpm_compile_expr_t *)mmpm_calloc(new_capacity, sizeof(mmpm_compile_expr_t));
    if (exprs == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    if (ctx->exprs != NULL) {
        (void)memcpy(exprs, ctx->exprs, ctx->expr_count * sizeof(mmpm_compile_expr_t));
        mmpm_free(ctx->exprs);
    }

    ctx->exprs = exprs;
    ctx->expr_capacity = new_capacity;
    return MMPM_SUCCESS;
}

static int mmpm_compile_context_reserve_combs(mmpm_compile_context_t *ctx)
{
    mmpm_compile_comb_t *combs = NULL;
    uint32_t new_capacity;

    if (ctx->comb_count < ctx->comb_capacity) {
        return MMPM_SUCCESS;
    }

    new_capacity = ctx->comb_capacity ? ctx->comb_capacity * 2 : 4;
    combs = (mmpm_compile_comb_t *)mmpm_calloc(new_capacity, sizeof(mmpm_compile_comb_t));
    if (combs == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    if (ctx->combs != NULL) {
        (void)memcpy(combs, ctx->combs, ctx->comb_count * sizeof(mmpm_compile_comb_t));
        mmpm_free(ctx->combs);
    }

    ctx->combs = combs;
    ctx->comb_capacity = new_capacity;
    return MMPM_SUCCESS;
}

static int mmpm_compile_context_add_expr(mmpm_compile_context_t *ctx,
                                         uint32_t id,
                                         uint32_t flags,
                                         uint32_t lkey)
{
    int ret = mmpm_compile_context_reserve_exprs(ctx);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ctx->exprs[ctx->expr_count].id = id;
    ctx->exprs[ctx->expr_count].flags = flags;
    ctx->exprs[ctx->expr_count].lkey = lkey;
    ctx->expr_count++;
    if (ctx->logical_count <= lkey) {
        ctx->logical_count = lkey + 1;
    }
    return MMPM_SUCCESS;
}

static int mmpm_compile_context_add_comb(mmpm_compile_context_t *ctx,
                                         uint32_t id,
                                         mmpm_logical_tree_t *tree)
{
    int ret = mmpm_compile_context_reserve_combs(ctx);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ctx->combs[ctx->comb_count].id = id;
    ctx->combs[ctx->comb_count].result = tree->result;
    ctx->combs[ctx->comb_count].tree = *tree;
    ctx->comb_count++;
    (void)memset(tree, 0, sizeof(*tree));
    tree->result = UINT32_MAX;
    return MMPM_SUCCESS;
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
                                           uint32_t elements)
{
    uint32_t i;
    int ret;

    for (i = 0; i < elements; i++) {
        mmpm_logical_tree_t tree;

        if (!mmpm_compile_is_combination(flags, i)) {
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

        ret = mmpm_compile_context_add_comb(ctx, ids ? ids[i] : 0, &tree);
        if (ret != MMPM_SUCCESS) {
            mmpm_logical_tree_destroy(&tree);
            return ret;
        }
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_append_context(mmpm_handler_t *handler,
                                       mmpm_compile_context_t *new_ctx)
{
    mmpm_compile_context_t *ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
    uint32_t i;
    int ret;

    if (new_ctx->expr_count == 0 && new_ctx->comb_count == 0) {
        return MMPM_SUCCESS;
    }

    if (ctx == NULL) {
        handler->mmpm_compile_ctx = new_ctx;
        return MMPM_SUCCESS;
    }

    for (i = 0; i < new_ctx->expr_count; i++) {
        ret = mmpm_compile_context_add_expr(ctx, new_ctx->exprs[i].id,
                                            new_ctx->exprs[i].flags,
                                            new_ctx->exprs[i].lkey);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    for (i = 0; i < new_ctx->comb_count; i++) {
        ret = mmpm_compile_context_add_comb(ctx, new_ctx->combs[i].id, &new_ctx->combs[i].tree);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_compile_multi(mmpm_handler_t *handler, const char *const *expressions, const uint32_t *flags,
                       const uint32_t *ids, uint32_t elements)
{
    uint32_t i;
    uint32_t plain_count = 0;
    int ret;
    mmpm_rule_t *rules = NULL;
    mmpm_compile_context_t *new_ctx = NULL;
    mmpm_compile_context_t *old_ctx = NULL;

    if (handler == NULL) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_compile_validate(expressions, flags, ids, elements);
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

    ret = mmpm_compile_parse_combinations(new_ctx, old_ctx, expressions, flags, ids, elements);
    if (ret != MMPM_SUCCESS) {
        mmpm_compile_context_free(new_ctx);
        return ret;
    }

    rules = (mmpm_rule_t *)mmpm_calloc(elements, sizeof(mmpm_rule_t));
    if (rules == NULL) {
        mmpm_compile_context_free(new_ctx);
        return MMPM_MALLOC_FAIL;
    }

    for (i = 0; i < elements; i++) {
        size_t len = strlen(expressions[i]);
        if (len == 0 || len > UINT32_MAX) {
            mmpm_free(rules);
            mmpm_compile_context_free(new_ctx);
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

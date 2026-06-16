/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>
#include <string.h>

#include "mmpm_allocator.h"
#include "mmpm_expr_internal.h"
#include "mmpm_logical.h"

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

static int mmpm_compile_context_reserve_lkey_relations(mmpm_compile_context_t *ctx,
                                                       uint32_t logical_count)
{
    uint32_t **new_lkey_to_ckeys = NULL;
    uint32_t *new_counts = NULL;
    uint32_t *new_capacities = NULL;

    if (ctx == NULL) {
        return MMPM_INVALID_PARA;
    }
    if (logical_count <= ctx->logical_count) {
        return MMPM_SUCCESS;
    }

    new_lkey_to_ckeys = (uint32_t **)mmpm_calloc(logical_count, sizeof(uint32_t *));
    new_counts = (uint32_t *)mmpm_calloc(logical_count, sizeof(uint32_t));
    new_capacities = (uint32_t *)mmpm_calloc(logical_count, sizeof(uint32_t));
    if (new_lkey_to_ckeys == NULL || new_counts == NULL || new_capacities == NULL) {
        if (new_lkey_to_ckeys != NULL) {
            mmpm_free(new_lkey_to_ckeys);
        }
        if (new_counts != NULL) {
            mmpm_free(new_counts);
        }
        if (new_capacities != NULL) {
            mmpm_free(new_capacities);
        }
        return MMPM_MALLOC_FAIL;
    }

    if (ctx->lkey_to_ckeys != NULL) {
        (void)memcpy(new_lkey_to_ckeys, ctx->lkey_to_ckeys, ctx->logical_count * sizeof(uint32_t *));
        mmpm_free(ctx->lkey_to_ckeys);
    }
    if (ctx->lkey_to_ckey_counts != NULL) {
        (void)memcpy(new_counts, ctx->lkey_to_ckey_counts, ctx->logical_count * sizeof(uint32_t));
        mmpm_free(ctx->lkey_to_ckey_counts);
    }
    if (ctx->lkey_to_ckey_capacities != NULL) {
        (void)memcpy(new_capacities, ctx->lkey_to_ckey_capacities, ctx->logical_count * sizeof(uint32_t));
        mmpm_free(ctx->lkey_to_ckey_capacities);
    }

    ctx->lkey_to_ckeys = new_lkey_to_ckeys;
    ctx->lkey_to_ckey_counts = new_counts;
    ctx->lkey_to_ckey_capacities = new_capacities;
    ctx->logical_count = logical_count;
    return MMPM_SUCCESS;
}

static int mmpm_compile_context_add_lkey_relation(mmpm_compile_context_t *ctx,
                                                  uint32_t lkey,
                                                  uint32_t ckey)
{
    uint32_t *new_ckeys = NULL;
    uint32_t i;
    uint32_t new_capacity;

    if (ctx == NULL) {
        return MMPM_INVALID_PARA;
    }
    if (lkey >= ctx->logical_count) {
        int ret = mmpm_compile_context_reserve_lkey_relations(ctx, lkey + 1);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    for (i = 0; i < ctx->lkey_to_ckey_counts[lkey]; i++) {
        if (ctx->lkey_to_ckeys[lkey][i] == ckey) {
            return MMPM_SUCCESS;
        }
    }

    if (ctx->lkey_to_ckey_counts[lkey] >= ctx->lkey_to_ckey_capacities[lkey]) {
        new_capacity = ctx->lkey_to_ckey_capacities[lkey] ? ctx->lkey_to_ckey_capacities[lkey] * 2 : 4;
        new_ckeys = (uint32_t *)mmpm_calloc(new_capacity, sizeof(uint32_t));
        if (new_ckeys == NULL) {
            return MMPM_MALLOC_FAIL;
        }
        if (ctx->lkey_to_ckeys[lkey] != NULL) {
            (void)memcpy(new_ckeys, ctx->lkey_to_ckeys[lkey],
                         ctx->lkey_to_ckey_counts[lkey] * sizeof(uint32_t));
            mmpm_free(ctx->lkey_to_ckeys[lkey]);
        }
        ctx->lkey_to_ckeys[lkey] = new_ckeys;
        ctx->lkey_to_ckey_capacities[lkey] = new_capacity;
    }

    ctx->lkey_to_ckeys[lkey][ctx->lkey_to_ckey_counts[lkey]++] = ckey;
    return MMPM_SUCCESS;
}

static int mmpm_compile_context_collect_comb_lkeys(const mmpm_logical_tree_t *tree,
                                                   uint32_t *lkey_start,
                                                   uint32_t *lkey_result)
{
    uint32_t i;
    uint32_t min_op_lkey = UINT32_MAX;

    if (tree == NULL || lkey_start == NULL || lkey_result == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < tree->op_count; i++) {
        if ((tree->ops[i].id & MMPM_LOGICAL_OP_REF) != 0) {
            return MMPM_INVALID_PARA;
        }
        if (tree->ops[i].id < min_op_lkey) {
            min_op_lkey = tree->ops[i].id;
        }
    }

    if (min_op_lkey == UINT32_MAX || (tree->result & MMPM_LOGICAL_OP_REF) != 0) {
        return MMPM_INVALID_PARA;
    }

    *lkey_start = min_op_lkey;
    *lkey_result = tree->result;
    return MMPM_SUCCESS;
}

static int mmpm_compile_context_add_comb_relations(mmpm_compile_context_t *ctx,
                                                   const mmpm_logical_tree_t *tree,
                                                   uint32_t ckey,
                                                   uint32_t op_lkey_start)
{
    uint32_t i;
    int ret;

    if (ctx == NULL || tree == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < tree->op_count; i++) {
        if ((tree->ops[i].right & MMPM_LOGICAL_OP_REF) == 0 &&
            tree->ops[i].right < op_lkey_start) {
            ret = mmpm_compile_context_add_lkey_relation(ctx, tree->ops[i].right, ckey);
            if (ret != MMPM_SUCCESS) {
                return ret;
            }
        }
        if (tree->ops[i].op != MMPM_LOGICAL_OP_NOT &&
            (tree->ops[i].left & MMPM_LOGICAL_OP_REF) == 0) {
            if (tree->ops[i].left >= op_lkey_start) {
                continue;
            }
            ret = mmpm_compile_context_add_lkey_relation(ctx, tree->ops[i].left, ckey);
            if (ret != MMPM_SUCCESS) {
                return ret;
            }
        }
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_context_reserve_tree_lkeys(mmpm_compile_context_t *ctx,
                                                   const mmpm_logical_tree_t *tree)
{
    uint32_t i;
    uint32_t max_lkey;

    if (ctx == NULL || tree == NULL || tree->op_count == 0) {
        return MMPM_INVALID_PARA;
    }

    max_lkey = tree->result;
    for (i = 0; i < tree->op_count; i++) {
        if ((tree->ops[i].id & MMPM_LOGICAL_OP_REF) != 0 ||
            (tree->ops[i].left & MMPM_LOGICAL_OP_REF) != 0 ||
            (tree->ops[i].right & MMPM_LOGICAL_OP_REF) != 0) {
            return MMPM_INVALID_PARA;
        }
        if (tree->ops[i].id > max_lkey) {
            max_lkey = tree->ops[i].id;
        }
    }

    return mmpm_compile_context_reserve_lkey_relations(ctx, max_lkey + 1);
}

static int mmpm_compile_context_detect_pure_negative(const mmpm_compile_context_t *ctx,
                                                     const mmpm_logical_tree_t *tree,
                                                     bool *can_match)
{
    uint8_t *logical_vector = NULL;
    uint8_t matched = 0;
    uint32_t bytes;
    int ret;

    if (ctx == NULL || tree == NULL || can_match == NULL) {
        return MMPM_INVALID_PARA;
    }

    *can_match = false;
    bytes = mmpm_compile_logical_byte_count(ctx->logical_count);
    if (bytes != 0) {
        logical_vector = (uint8_t *)mmpm_calloc(bytes, sizeof(uint8_t));
        if (logical_vector == NULL) {
            return MMPM_MALLOC_FAIL;
        }
    }

    ret = mmpm_logical_eval_inplace(tree, logical_vector, ctx->logical_count, &matched);
    if (logical_vector != NULL) {
        mmpm_free(logical_vector);
    }
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    *can_match = matched ? true : false;
    return MMPM_SUCCESS;
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

int mmpm_compile_context_add_expr(mmpm_compile_context_t *ctx,
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
        ret = mmpm_compile_context_reserve_lkey_relations(ctx, lkey + 1);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
        ctx->logical_count = lkey + 1;
    }
    return MMPM_SUCCESS;
}

int mmpm_compile_context_add_comb(mmpm_compile_context_t *ctx,
                                  uint32_t id,
                                  uint32_t flags,
                                  const mmpm_expr_ext_t *ext,
                                  mmpm_logical_tree_t *tree)
{
    int ret = mmpm_compile_context_reserve_combs(ctx);
    uint32_t lkey_start;
    uint32_t lkey_result;
    uint32_t ckey = ctx->comb_count;
    uint32_t ekey = UINT32_MAX;
    uint64_t min_offset = 0;
    uint64_t max_offset = UINT64_MAX;
    bool can_match_without_active_lkey = false;

    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = mmpm_compile_context_collect_comb_lkeys(tree, &lkey_start, &lkey_result);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = mmpm_compile_context_reserve_tree_lkeys(ctx, tree);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = mmpm_compile_context_detect_pure_negative(ctx, tree, &can_match_without_active_lkey);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    if ((flags & MMPM_FLAG_SINGLEMATCH) != 0) {
        ekey = ctx->ekey_count++;
    }

    if (ext != NULL) {
        if ((ext->flags & MMPM_EXT_FLAG_MIN_OFFSET) != 0) {
            min_offset = ext->min_offset;
        }
        if ((ext->flags & MMPM_EXT_FLAG_MAX_OFFSET) != 0) {
            max_offset = ext->max_offset;
        }
    }

    ctx->combs[ctx->comb_count].id = id;
    ctx->combs[ctx->comb_count].flags = flags;
    ctx->combs[ctx->comb_count].ckey = ckey;
    ctx->combs[ctx->comb_count].ekey = ekey;
    ctx->combs[ctx->comb_count].lkey_start = lkey_start;
    ctx->combs[ctx->comb_count].lkey_result = lkey_result;
    ctx->combs[ctx->comb_count].result = tree->result;
    ctx->combs[ctx->comb_count].min_offset = min_offset;
    ctx->combs[ctx->comb_count].max_offset = max_offset;
    ctx->combs[ctx->comb_count].can_match_without_active_lkey = can_match_without_active_lkey;
    ctx->combs[ctx->comb_count].tree = *tree;
    ctx->comb_count++;
    ret = mmpm_compile_context_add_comb_relations(ctx, tree, ckey, lkey_start);
    if (ret != MMPM_SUCCESS) {
        ctx->comb_count--;
        if (ekey != UINT32_MAX && ctx->ekey_count != 0) {
            ctx->ekey_count--;
        }
        return ret;
    }
    (void)memset(tree, 0, sizeof(*tree));
    tree->result = UINT32_MAX;
    return MMPM_SUCCESS;
}

static bool mmpm_compile_tree_uses_lkey(const mmpm_logical_tree_t *tree, uint32_t lkey)
{
    uint32_t i;

    if (tree == NULL) {
        return false;
    }

    for (i = 0; i < tree->op_count; i++) {
        if ((tree->ops[i].right & MMPM_LOGICAL_OP_REF) == 0 &&
            tree->ops[i].right == lkey) {
            return true;
        }
        if (tree->ops[i].op != MMPM_LOGICAL_OP_NOT &&
            (tree->ops[i].left & MMPM_LOGICAL_OP_REF) == 0 &&
            tree->ops[i].left == lkey) {
            return true;
        }
    }

    return (tree->result & MMPM_LOGICAL_OP_REF) == 0 && tree->result == lkey;
}

static void mmpm_compile_context_reassign_comb_keys(mmpm_compile_context_t *ctx)
{
    uint32_t i;
    uint32_t ekey = 0;

    if (ctx == NULL) {
        return;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        ctx->combs[i].ckey = i;
        if ((ctx->combs[i].flags & MMPM_FLAG_SINGLEMATCH) != 0) {
            ctx->combs[i].ekey = ekey++;
        } else {
            ctx->combs[i].ekey = UINT32_MAX;
        }
    }

    ctx->ekey_count = ekey;
}

static void mmpm_compile_context_clear_lkey_relations(mmpm_compile_context_t *ctx)
{
    uint32_t i;

    if (ctx == NULL) {
        return;
    }

    if (ctx->lkey_to_ckeys != NULL) {
        for (i = 0; i < ctx->logical_count; i++) {
            if (ctx->lkey_to_ckeys[i] != NULL) {
                mmpm_free(ctx->lkey_to_ckeys[i]);
                ctx->lkey_to_ckeys[i] = NULL;
            }
        }
    }
    if (ctx->lkey_to_ckey_counts != NULL) {
        (void)memset(ctx->lkey_to_ckey_counts, 0, ctx->logical_count * sizeof(uint32_t));
    }
    if (ctx->lkey_to_ckey_capacities != NULL) {
        (void)memset(ctx->lkey_to_ckey_capacities, 0, ctx->logical_count * sizeof(uint32_t));
    }
}

static int mmpm_compile_context_rebuild_comb_relations(mmpm_compile_context_t *ctx)
{
    uint32_t i;
    int ret;

    if (ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    mmpm_compile_context_clear_lkey_relations(ctx);
    mmpm_compile_context_reassign_comb_keys(ctx);
    for (i = 0; i < ctx->comb_count; i++) {
        ret = mmpm_compile_context_add_comb_relations(ctx, &ctx->combs[i].tree,
                                                      i, ctx->combs[i].lkey_start);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    return MMPM_SUCCESS;
}

static void mmpm_compile_context_remove_comb_at(mmpm_compile_context_t *ctx, uint32_t index)
{
    uint32_t i;

    if (ctx == NULL || index >= ctx->comb_count) {
        return;
    }

    mmpm_logical_tree_destroy(&ctx->combs[index].tree);
    for (i = index + 1; i < ctx->comb_count; i++) {
        ctx->combs[i - 1] = ctx->combs[i];
    }
    ctx->comb_count--;
    (void)memset(&ctx->combs[ctx->comb_count], 0, sizeof(ctx->combs[ctx->comb_count]));
}

int mmpm_compile_context_remove_comb_id(mmpm_compile_context_t *ctx, uint32_t id)
{
    uint32_t i;

    if (ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        if (ctx->combs[i].id == id) {
            mmpm_compile_context_remove_comb_at(ctx, i);
            return mmpm_compile_context_rebuild_comb_relations(ctx);
        }
    }

    return MMPM_FIND_FAIL;
}

static int mmpm_compile_context_remove_combs_using_lkey(mmpm_compile_context_t *ctx, uint32_t lkey)
{
    uint32_t i;

    if (ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (i = ctx->comb_count; i > 0; i--) {
        if (mmpm_compile_tree_uses_lkey(&ctx->combs[i - 1].tree, lkey)) {
            mmpm_compile_context_remove_comb_at(ctx, i - 1);
        }
    }

    return mmpm_compile_context_rebuild_comb_relations(ctx);
}

static int mmpm_compile_context_remove_expr_id(mmpm_compile_context_t *ctx, uint32_t id)
{
    uint32_t i;
    uint32_t lkey;
    int ret;

    if (ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < ctx->expr_count; i++) {
        if (ctx->exprs[i].id != id) {
            continue;
        }

        lkey = ctx->exprs[i].lkey;
        ret = mmpm_compile_context_remove_combs_using_lkey(ctx, lkey);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }

        if (i + 1 < ctx->expr_count) {
            (void)memmove(&ctx->exprs[i], &ctx->exprs[i + 1],
                          (ctx->expr_count - i - 1) * sizeof(ctx->exprs[i]));
        }
        ctx->expr_count--;
        (void)memset(&ctx->exprs[ctx->expr_count], 0, sizeof(ctx->exprs[ctx->expr_count]));
        return MMPM_SUCCESS;
    }

    return MMPM_FIND_FAIL;
}

int mmpm_compile_context_remove_id(mmpm_compile_context_t *ctx, uint32_t id)
{
    int ret;

    if (ctx == NULL) {
        return MMPM_FIND_FAIL;
    }

    ret = mmpm_compile_context_remove_expr_id(ctx, id);
    if (ret == MMPM_SUCCESS) {
        return MMPM_SUCCESS;
    }
    if (ret != MMPM_FIND_FAIL) {
        return ret;
    }

    return mmpm_compile_context_remove_comb_id(ctx, id);
}

int mmpm_compile_append_context(mmpm_handler_t *handler,
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
        mmpm_expr_ext_t ext = {0};

        ext.flags = MMPM_EXT_FLAG_MIN_OFFSET | MMPM_EXT_FLAG_MAX_OFFSET;
        ext.min_offset = new_ctx->combs[i].min_offset;
        ext.max_offset = new_ctx->combs[i].max_offset;
        ret = mmpm_compile_context_add_comb(ctx, new_ctx->combs[i].id,
                                            new_ctx->combs[i].flags,
                                            &ext,
                                            &new_ctx->combs[i].tree);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
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
    if (ctx->lkey_to_ckeys != NULL) {
        for (i = 0; i < ctx->logical_count; i++) {
            if (ctx->lkey_to_ckeys[i] != NULL) {
                mmpm_free(ctx->lkey_to_ckeys[i]);
            }
        }
        mmpm_free(ctx->lkey_to_ckeys);
    }
    if (ctx->lkey_to_ckey_counts != NULL) {
        mmpm_free(ctx->lkey_to_ckey_counts);
    }
    if (ctx->lkey_to_ckey_capacities != NULL) {
        mmpm_free(ctx->lkey_to_ckey_capacities);
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

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>
#include <string.h>

#include "ms_allocator.h"
#include "../ms_database_internal.h"
#include "ms_expr_internal.h"
#include "ms_logical.h"

bool ms_compile_context_has_id(const ms_compile_context_t *ctx, uint32_t id)
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

bool ms_compile_context_has_comb_id(const ms_compile_context_t *ctx, uint32_t id)
{
    uint32_t i;

    if (ctx == NULL) {
        return false;
    }
    for (i = 0; i < ctx->comb_count; i++) {
        if (ctx->combs[i].id == id) {
            return true;
        }
    }
    return false;
}

int ms_compile_context_find_lkey(const ms_compile_context_t *ctx,
                                   uint32_t id,
                                   uint32_t *lkey)
{
    uint32_t i;

    if (ctx == NULL || lkey == NULL) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < ctx->expr_count; i++) {
        if (ctx->exprs[i].id == id) {
            *lkey = ctx->exprs[i].lkey;
            return MS_SUCCESS;
        }
    }

    return MS_FIND_FAIL;
}

const ms_compile_expr_t *ms_compile_context_find_expr(const ms_compile_context_t *ctx,
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

const ms_compile_expr_t *ms_compile_context_find_expr_by_lkey(
    const ms_compile_context_t *ctx, uint32_t lkey)
{
    uint32_t index;

    if (ctx == NULL || lkey >= ctx->logical_count ||
        ctx->lkey_to_expr_indexes == NULL) {
        return NULL;
    }

    index = ctx->lkey_to_expr_indexes[lkey];
    if (index == UINT32_MAX || index >= ctx->expr_count ||
        ctx->exprs[index].lkey != lkey) {
        return NULL;
    }
    return &ctx->exprs[index];
}

const ms_compile_expr_t *ms_compile_context_find_expr_for_alias(
    const ms_compile_context_t *ctx, uint32_t alias)
{
    if (ctx == NULL) {
        return NULL;
    }
    if (ctx->ac_aliases_are_lkeys) {
        return ms_compile_context_find_expr_by_lkey(ctx, alias);
    }
    return ms_compile_context_find_expr(ctx, alias);
}

int ms_compile_context_alias_to_lkey(const ms_compile_context_t *ctx,
                                       uint32_t alias,
                                       uint32_t *lkey)
{
    const ms_compile_expr_t *expr;

    if (ctx == NULL || lkey == NULL) {
        return MS_INVALID_PARA;
    }
    if (ctx->ac_aliases_are_lkeys) {
        expr = ms_compile_context_find_expr_by_lkey(ctx, alias);
        if (expr == NULL) {
            return MS_FIND_FAIL;
        }
        *lkey = alias;
        return MS_SUCCESS;
    }
    return ms_compile_context_find_lkey(ctx, alias, lkey);
}

uint32_t ms_compile_logical_byte_count(uint32_t logical_count)
{
    return (logical_count + 8u - 1) / 8u;
}

static int ms_compile_context_reserve_lkey_relations(ms_compile_context_t *ctx,
                                                       uint32_t logical_count)
{
    uint32_t **new_lkey_to_ckeys = NULL;
    uint32_t *new_counts = NULL;
    uint32_t *new_capacities = NULL;
    uint32_t *new_expr_indexes = NULL;
    uint32_t new_capacity;
    uint32_t i;

    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }
    if (logical_count <= ctx->logical_count) {
        return MS_SUCCESS;
    }
    if (logical_count <= ctx->lkey_to_ckey_capacity) {
        ctx->logical_count = logical_count;
        return MS_SUCCESS;
    }

    if (ctx->lkey_to_ckey_capacity > UINT32_MAX / 2u) {
        new_capacity = logical_count;
    } else {
        new_capacity = ctx->lkey_to_ckey_capacity
                           ? ctx->lkey_to_ckey_capacity * 2u
                           : 4u;
    }
    while (new_capacity < logical_count) {
        if (new_capacity > UINT32_MAX / 2u) {
            new_capacity = logical_count;
            break;
        }
        new_capacity *= 2u;
    }

    new_lkey_to_ckeys = (uint32_t **)ms_calloc(new_capacity, sizeof(uint32_t *));
    new_counts = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
    new_capacities = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
    new_expr_indexes = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
    if (new_lkey_to_ckeys == NULL || new_counts == NULL ||
        new_capacities == NULL || new_expr_indexes == NULL) {
        if (new_lkey_to_ckeys != NULL) {
            ms_free(new_lkey_to_ckeys);
        }
        if (new_counts != NULL) {
            ms_free(new_counts);
        }
        if (new_capacities != NULL) {
            ms_free(new_capacities);
        }
        if (new_expr_indexes != NULL) {
            ms_free(new_expr_indexes);
        }
        return MS_MALLOC_FAIL;
    }
    for (i = 0; i < new_capacity; i++) {
        new_expr_indexes[i] = UINT32_MAX;
    }

    if (ctx->lkey_to_ckeys != NULL) {
        (void)memcpy(new_lkey_to_ckeys, ctx->lkey_to_ckeys, ctx->logical_count * sizeof(uint32_t *));
        ms_free(ctx->lkey_to_ckeys);
    }
    if (ctx->lkey_to_ckey_counts != NULL) {
        (void)memcpy(new_counts, ctx->lkey_to_ckey_counts, ctx->logical_count * sizeof(uint32_t));
        ms_free(ctx->lkey_to_ckey_counts);
    }
    if (ctx->lkey_to_ckey_capacities != NULL) {
        (void)memcpy(new_capacities, ctx->lkey_to_ckey_capacities, ctx->logical_count * sizeof(uint32_t));
        ms_free(ctx->lkey_to_ckey_capacities);
    }
    if (ctx->lkey_to_expr_indexes != NULL) {
        (void)memcpy(new_expr_indexes, ctx->lkey_to_expr_indexes,
                     ctx->logical_count * sizeof(uint32_t));
        ms_free(ctx->lkey_to_expr_indexes);
    }

    ctx->lkey_to_ckeys = new_lkey_to_ckeys;
    ctx->lkey_to_ckey_counts = new_counts;
    ctx->lkey_to_ckey_capacities = new_capacities;
    ctx->lkey_to_expr_indexes = new_expr_indexes;
    ctx->logical_count = logical_count;
    ctx->lkey_to_ckey_capacity = new_capacity;
    return MS_SUCCESS;
}

static int ms_compile_context_add_lkey_relation(ms_compile_context_t *ctx,
                                                  uint32_t lkey,
                                                  uint32_t ckey)
{
    uint32_t *new_ckeys = NULL;
    uint32_t i;
    uint32_t new_capacity;

    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }
    if (lkey >= ctx->logical_count) {
        int ret = ms_compile_context_reserve_lkey_relations(ctx, lkey + 1);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    for (i = 0; i < ctx->lkey_to_ckey_counts[lkey]; i++) {
        if (ctx->lkey_to_ckeys[lkey][i] == ckey) {
            return MS_SUCCESS;
        }
    }

    if (ctx->lkey_to_ckey_counts[lkey] >= ctx->lkey_to_ckey_capacities[lkey]) {
        if (ctx->lkey_to_ckey_capacities[lkey] > UINT32_MAX / 2u) {
            return MS_MALLOC_FAIL;
        }
        new_capacity = ctx->lkey_to_ckey_capacities[lkey] ? ctx->lkey_to_ckey_capacities[lkey] * 2 : 4;
        new_ckeys = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
        if (new_ckeys == NULL) {
            return MS_MALLOC_FAIL;
        }
        if (ctx->lkey_to_ckeys[lkey] != NULL) {
            (void)memcpy(new_ckeys, ctx->lkey_to_ckeys[lkey],
                         ctx->lkey_to_ckey_counts[lkey] * sizeof(uint32_t));
            ms_free(ctx->lkey_to_ckeys[lkey]);
        }
        ctx->lkey_to_ckeys[lkey] = new_ckeys;
        ctx->lkey_to_ckey_capacities[lkey] = new_capacity;
    }

    ctx->lkey_to_ckeys[lkey][ctx->lkey_to_ckey_counts[lkey]++] = ckey;
    return MS_SUCCESS;
}

static int ms_compile_context_collect_comb_lkeys(const ms_logical_tree_t *tree,
                                                   uint32_t *lkey_start,
                                                   uint32_t *lkey_result)
{
    uint32_t i;
    uint32_t min_op_lkey = UINT32_MAX;

    if (tree == NULL || lkey_start == NULL || lkey_result == NULL) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < tree->op_count; i++) {
        if ((tree->ops[i].id & MS_LOGICAL_OP_REF) != 0) {
            return MS_INVALID_PARA;
        }
        if (tree->ops[i].id < min_op_lkey) {
            min_op_lkey = tree->ops[i].id;
        }
    }

    if (min_op_lkey == UINT32_MAX || (tree->result & MS_LOGICAL_OP_REF) != 0) {
        return MS_INVALID_PARA;
    }

    *lkey_start = min_op_lkey;
    *lkey_result = tree->result;
    return MS_SUCCESS;
}

static int ms_compile_context_add_comb_relations(ms_compile_context_t *ctx,
                                                   const ms_logical_tree_t *tree,
                                                   uint32_t ckey,
                                                   uint32_t op_lkey_start)
{
    uint32_t i;
    int ret;

    if (ctx == NULL || tree == NULL) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < tree->op_count; i++) {
        if ((tree->ops[i].right & MS_LOGICAL_OP_REF) == 0 &&
            tree->ops[i].right < op_lkey_start) {
            ret = ms_compile_context_add_lkey_relation(ctx, tree->ops[i].right, ckey);
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }
        if (tree->ops[i].op != MS_LOGICAL_OP_NOT &&
            (tree->ops[i].left & MS_LOGICAL_OP_REF) == 0) {
            if (tree->ops[i].left >= op_lkey_start) {
                continue;
            }
            ret = ms_compile_context_add_lkey_relation(ctx, tree->ops[i].left, ckey);
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }
    }

    return MS_SUCCESS;
}

static int ms_compile_context_reserve_tree_lkeys(ms_compile_context_t *ctx,
                                                   const ms_logical_tree_t *tree)
{
    uint32_t i;
    uint32_t max_lkey;

    if (ctx == NULL || tree == NULL || tree->op_count == 0) {
        return MS_INVALID_PARA;
    }

    max_lkey = tree->result;
    for (i = 0; i < tree->op_count; i++) {
        if ((tree->ops[i].id & MS_LOGICAL_OP_REF) != 0 ||
            (tree->ops[i].left & MS_LOGICAL_OP_REF) != 0 ||
            (tree->ops[i].right & MS_LOGICAL_OP_REF) != 0) {
            return MS_INVALID_PARA;
        }
        if (tree->ops[i].id > max_lkey) {
            max_lkey = tree->ops[i].id;
        }
    }

    return ms_compile_context_reserve_lkey_relations(ctx, max_lkey + 1);
}

static int ms_compile_context_detect_pure_negative(const ms_compile_context_t *ctx,
                                                     const ms_logical_tree_t *tree,
                                                     bool *can_match)
{
    uint8_t *logical_vector = NULL;
    uint8_t matched = 0;
    uint32_t bytes;
    int ret;

    if (ctx == NULL || tree == NULL || can_match == NULL) {
        return MS_INVALID_PARA;
    }

    *can_match = false;
    bytes = ms_compile_logical_byte_count(ctx->logical_count);
    if (bytes != 0) {
        logical_vector = (uint8_t *)ms_calloc(bytes, sizeof(uint8_t));
        if (logical_vector == NULL) {
            return MS_MALLOC_FAIL;
        }
    }

    ret = ms_logical_eval_inplace(tree, logical_vector, ctx->logical_count, &matched);
    if (logical_vector != NULL) {
        ms_free(logical_vector);
    }
    if (ret != MS_SUCCESS) {
        return ret;
    }

    *can_match = matched ? true : false;
    return MS_SUCCESS;
}

static int ms_compile_context_reserve_exprs(ms_compile_context_t *ctx)
{
    ms_compile_expr_t *exprs = NULL;
    uint32_t new_capacity;

    if (ctx->expr_count < ctx->expr_capacity) {
        return MS_SUCCESS;
    }

    if (ctx->expr_capacity > UINT32_MAX / 2u) {
        return MS_MALLOC_FAIL;
    }
    new_capacity = ctx->expr_capacity ? ctx->expr_capacity * 2u : 4u;
    exprs = (ms_compile_expr_t *)ms_calloc(new_capacity, sizeof(ms_compile_expr_t));
    if (exprs == NULL) {
        return MS_MALLOC_FAIL;
    }

    if (ctx->exprs != NULL) {
        (void)memcpy(exprs, ctx->exprs, ctx->expr_count * sizeof(ms_compile_expr_t));
        ms_free(ctx->exprs);
    }

    ctx->exprs = exprs;
    ctx->expr_capacity = new_capacity;
    return MS_SUCCESS;
}

static int ms_compile_context_reserve_combs(ms_compile_context_t *ctx)
{
    ms_compile_comb_t *combs = NULL;
    uint32_t new_capacity;

    if (ctx->comb_count < ctx->comb_capacity) {
        return MS_SUCCESS;
    }

    if (ctx->comb_capacity > UINT32_MAX / 2u) {
        return MS_MALLOC_FAIL;
    }
    new_capacity = ctx->comb_capacity ? ctx->comb_capacity * 2u : 4u;
    combs = (ms_compile_comb_t *)ms_calloc(new_capacity, sizeof(ms_compile_comb_t));
    if (combs == NULL) {
        return MS_MALLOC_FAIL;
    }

    if (ctx->combs != NULL) {
        (void)memcpy(combs, ctx->combs, ctx->comb_count * sizeof(ms_compile_comb_t));
        ms_free(ctx->combs);
    }

    ctx->combs = combs;
    ctx->comb_capacity = new_capacity;
    return MS_SUCCESS;
}

static int ms_compile_context_reserve_final_ckeys(ms_compile_context_t *ctx)
{
    uint32_t *final_ckeys = NULL;
    uint32_t new_capacity;

    if (ctx->final_ckey_count < ctx->final_ckey_capacity) {
        return MS_SUCCESS;
    }

    if (ctx->final_ckey_capacity > UINT32_MAX / 2u) {
        return MS_MALLOC_FAIL;
    }
    new_capacity = ctx->final_ckey_capacity
                       ? ctx->final_ckey_capacity * 2u
                       : 4u;
    final_ckeys = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
    if (final_ckeys == NULL) {
        return MS_MALLOC_FAIL;
    }

    if (ctx->final_ckeys != NULL) {
        (void)memcpy(final_ckeys, ctx->final_ckeys,
                     ctx->final_ckey_count * sizeof(uint32_t));
        ms_free(ctx->final_ckeys);
    }

    ctx->final_ckeys = final_ckeys;
    ctx->final_ckey_capacity = new_capacity;
    return MS_SUCCESS;
}

int ms_compile_context_add_expr(ms_compile_context_t *ctx,
                                  uint32_t id,
                                  uint32_t flags,
                                  uint32_t lkey,
                                  uint32_t pattern_len)
{
    int ret = ms_compile_context_reserve_exprs(ctx);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    if (ctx->logical_count <= lkey) {
        ret = ms_compile_context_reserve_lkey_relations(ctx, lkey + 1);
        if (ret != MS_SUCCESS) {
            return ret;
        }
        ctx->logical_count = lkey + 1;
    }
    if (ctx->lkey_to_expr_indexes == NULL ||
        ctx->lkey_to_expr_indexes[lkey] != UINT32_MAX) {
        return MS_INVALID_PARA;
    }

    ctx->exprs[ctx->expr_count].id = id;
    ctx->exprs[ctx->expr_count].flags = flags;
    ctx->exprs[ctx->expr_count].lkey = lkey;
    ctx->exprs[ctx->expr_count].pattern_len = pattern_len;
    ctx->lkey_to_expr_indexes[lkey] = ctx->expr_count;
    ctx->expr_count++;
    return MS_SUCCESS;
}

int ms_compile_context_add_comb(ms_compile_context_t *ctx,
                                  uint32_t id,
                                  uint32_t flags,
                                  const ms_expr_ext_t *ext,
                                  ms_logical_tree_t *tree)
{
    int ret = ms_compile_context_reserve_combs(ctx);
    uint32_t lkey_start;
    uint32_t lkey_result;
    uint32_t ckey = ctx->comb_count;
    uint32_t ekey = UINT32_MAX;
    uint64_t min_offset = 0;
    uint64_t max_offset = UINT64_MAX;
    bool can_match_without_active_lkey = false;

    if (ret != MS_SUCCESS) {
        return ret;
    }

    ret = ms_compile_context_collect_comb_lkeys(tree, &lkey_start, &lkey_result);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    ret = ms_compile_context_reserve_tree_lkeys(ctx, tree);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    ret = ms_compile_context_detect_pure_negative(ctx, tree, &can_match_without_active_lkey);
    if (ret != MS_SUCCESS) {
        return ret;
    }
    if (can_match_without_active_lkey) {
        ret = ms_compile_context_reserve_final_ckeys(ctx);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    if ((flags & MS_FLAG_SINGLEMATCH) != 0) {
        ekey = ctx->ekey_count++;
    }

    if (ext != NULL) {
        if ((ext->flags & MS_EXT_FLAG_MIN_OFFSET) != 0) {
            min_offset = ext->min_offset;
        }
        if ((ext->flags & MS_EXT_FLAG_MAX_OFFSET) != 0) {
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
    ret = ms_compile_context_add_comb_relations(ctx, tree, ckey, lkey_start);
    if (ret != MS_SUCCESS) {
        ctx->comb_count--;
        if (ekey != UINT32_MAX && ctx->ekey_count != 0) {
            ctx->ekey_count--;
        }
        return ret;
    }
    if (can_match_without_active_lkey) {
        ctx->final_ckeys[ctx->final_ckey_count++] = ckey;
    }
    (void)memset(tree, 0, sizeof(*tree));
    tree->result = UINT32_MAX;
    return MS_SUCCESS;
}

static bool ms_compile_tree_uses_lkey(const ms_logical_tree_t *tree, uint32_t lkey)
{
    uint32_t i;

    if (tree == NULL) {
        return false;
    }

    for (i = 0; i < tree->op_count; i++) {
        if ((tree->ops[i].right & MS_LOGICAL_OP_REF) == 0 &&
            tree->ops[i].right == lkey) {
            return true;
        }
        if (tree->ops[i].op != MS_LOGICAL_OP_NOT &&
            (tree->ops[i].left & MS_LOGICAL_OP_REF) == 0 &&
            tree->ops[i].left == lkey) {
            return true;
        }
    }

    return (tree->result & MS_LOGICAL_OP_REF) == 0 && tree->result == lkey;
}

static void ms_compile_context_reassign_comb_keys(ms_compile_context_t *ctx)
{
    uint32_t i;
    uint32_t ekey = 0;

    if (ctx == NULL) {
        return;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        ctx->combs[i].ckey = i;
        if ((ctx->combs[i].flags & MS_FLAG_SINGLEMATCH) != 0) {
            ctx->combs[i].ekey = ekey++;
        } else {
            ctx->combs[i].ekey = UINT32_MAX;
        }
    }

    ctx->ekey_count = ekey;
}

static void ms_compile_context_rebuild_final_ckeys(ms_compile_context_t *ctx)
{
    uint32_t i;

    if (ctx == NULL) {
        return;
    }

    ctx->final_ckey_count = 0;
    for (i = 0; i < ctx->comb_count; i++) {
        if (ctx->combs[i].can_match_without_active_lkey) {
            ctx->final_ckeys[ctx->final_ckey_count++] = ctx->combs[i].ckey;
        }
    }
}

static void ms_compile_context_clear_lkey_relations(ms_compile_context_t *ctx)
{
    uint32_t i;

    if (ctx == NULL) {
        return;
    }

    if (ctx->lkey_to_ckeys != NULL) {
        for (i = 0; i < ctx->logical_count; i++) {
            if (ctx->lkey_to_ckeys[i] != NULL) {
                ms_free(ctx->lkey_to_ckeys[i]);
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

static int ms_compile_context_rebuild_comb_relations(ms_compile_context_t *ctx)
{
    uint32_t i;
    int ret;

    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }

    ms_compile_context_clear_lkey_relations(ctx);
    ms_compile_context_reassign_comb_keys(ctx);
    ms_compile_context_rebuild_final_ckeys(ctx);
    for (i = 0; i < ctx->comb_count; i++) {
        ret = ms_compile_context_add_comb_relations(ctx, &ctx->combs[i].tree,
                                                      i, ctx->combs[i].lkey_start);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    return MS_SUCCESS;
}

static void ms_compile_context_remove_comb_at(ms_compile_context_t *ctx, uint32_t index)
{
    uint32_t i;

    if (ctx == NULL || index >= ctx->comb_count) {
        return;
    }

    ms_logical_tree_destroy(&ctx->combs[index].tree);
    for (i = index + 1; i < ctx->comb_count; i++) {
        ctx->combs[i - 1] = ctx->combs[i];
    }
    ctx->comb_count--;
    (void)memset(&ctx->combs[ctx->comb_count], 0, sizeof(ctx->combs[ctx->comb_count]));
}

int ms_compile_context_remove_comb_id(ms_compile_context_t *ctx, uint32_t id)
{
    uint32_t i;

    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        if (ctx->combs[i].id == id) {
            ms_compile_context_remove_comb_at(ctx, i);
            return ms_compile_context_rebuild_comb_relations(ctx);
        }
    }

    return MS_FIND_FAIL;
}

static int ms_compile_context_remove_combs_using_lkey(ms_compile_context_t *ctx, uint32_t lkey)
{
    uint32_t i;

    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }

    for (i = ctx->comb_count; i > 0; i--) {
        if (ms_compile_tree_uses_lkey(&ctx->combs[i - 1].tree, lkey)) {
            ms_compile_context_remove_comb_at(ctx, i - 1);
        }
    }

    return ms_compile_context_rebuild_comb_relations(ctx);
}

static int ms_compile_context_remove_expr_id(ms_compile_context_t *ctx, uint32_t id)
{
    uint32_t i;
    uint32_t lkey;
    int ret;

    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < ctx->expr_count; i++) {
        if (ctx->exprs[i].id != id) {
            continue;
        }

        lkey = ctx->exprs[i].lkey;
        ret = ms_compile_context_remove_combs_using_lkey(ctx, lkey);
        if (ret != MS_SUCCESS) {
            return ret;
        }

        if (i + 1 < ctx->expr_count) {
            (void)memmove(&ctx->exprs[i], &ctx->exprs[i + 1],
                          (ctx->expr_count - i - 1) * sizeof(ctx->exprs[i]));
        }
        ctx->expr_count--;
        (void)memset(&ctx->exprs[ctx->expr_count], 0, sizeof(ctx->exprs[ctx->expr_count]));
        if (ctx->lkey_to_expr_indexes != NULL) {
            uint32_t key;

            ctx->lkey_to_expr_indexes[lkey] = UINT32_MAX;
            for (key = 0; key < ctx->logical_count; key++) {
                if (ctx->lkey_to_expr_indexes[key] != UINT32_MAX &&
                    ctx->lkey_to_expr_indexes[key] > i) {
                    ctx->lkey_to_expr_indexes[key]--;
                }
            }
        }
        return MS_SUCCESS;
    }

    return MS_FIND_FAIL;
}

int ms_compile_context_remove_id(ms_compile_context_t *ctx, uint32_t id)
{
    int ret;

    if (ctx == NULL) {
        return MS_FIND_FAIL;
    }

    ret = ms_compile_context_remove_expr_id(ctx, id);
    if (ret == MS_SUCCESS) {
        return MS_SUCCESS;
    }
    if (ret != MS_FIND_FAIL) {
        return ret;
    }

    return ms_compile_context_remove_comb_id(ctx, id);
}

static int ms_compile_logical_tree_clone(const ms_logical_tree_t *tree,
                                         ms_logical_tree_t *clone)
{
    if (tree == NULL || clone == NULL) {
        return MS_INVALID_PARA;
    }
    (void)memset(clone, 0, sizeof(*clone));
    clone->result = tree->result;
    if (tree->op_count == 0) {
        return MS_SUCCESS;
    }
    if (tree->ops == NULL) {
        return MS_INVALID_PARA;
    }
    clone->ops = (ms_logical_op_t *)ms_calloc(tree->op_count,
                                               sizeof(*clone->ops));
    if (clone->ops == NULL) {
        return MS_MALLOC_FAIL;
    }
    (void)memcpy(clone->ops, tree->ops,
                 tree->op_count * sizeof(*tree->ops));
    clone->op_count = tree->op_count;
    clone->op_capacity = tree->op_count;
    return MS_SUCCESS;
}

int ms_compile_context_clone(const ms_compile_context_t *ctx,
                              ms_compile_context_t **clone)
{
    ms_compile_context_t *new_ctx;
    uint32_t i;
    int ret;

    if (ctx == NULL || clone == NULL || *clone != NULL) {
        return MS_INVALID_PARA;
    }
    new_ctx = (ms_compile_context_t *)ms_calloc(1, sizeof(*new_ctx));
    if (new_ctx == NULL) {
        return MS_MALLOC_FAIL;
    }
    new_ctx->ac_aliases_are_lkeys = ctx->ac_aliases_are_lkeys;
    for (i = 0; i < ctx->expr_count; i++) {
        ret = ms_compile_context_add_expr(new_ctx, ctx->exprs[i].id,
                                           ctx->exprs[i].flags,
                                           ctx->exprs[i].lkey,
                                           ctx->exprs[i].pattern_len);
        if (ret != MS_SUCCESS) {
            ms_compile_context_free(new_ctx);
            return ret;
        }
    }
    for (i = 0; i < ctx->comb_count; i++) {
        ms_logical_tree_t tree;
        ms_expr_ext_t ext = {
            MS_EXT_FLAG_MIN_OFFSET | MS_EXT_FLAG_MAX_OFFSET,
            ctx->combs[i].min_offset,
            ctx->combs[i].max_offset
        };

        ret = ms_compile_logical_tree_clone(&ctx->combs[i].tree, &tree);
        if (ret == MS_SUCCESS) {
            ret = ms_compile_context_add_comb(new_ctx, ctx->combs[i].id,
                                               ctx->combs[i].flags,
                                               &ext, &tree);
        }
        ms_logical_tree_destroy(&tree);
        if (ret != MS_SUCCESS) {
            ms_compile_context_free(new_ctx);
            return ret;
        }
    }
    *clone = new_ctx;
    return MS_SUCCESS;
}

static int ms_compile_append_context_impl(ms_compile_context_t **ctx_slot,
                                            ms_compile_context_t *new_ctx)
{
    ms_compile_context_t *ctx = ctx_slot ? *ctx_slot : NULL;
    uint32_t i;
    int ret;

    if (ctx_slot == NULL || new_ctx == NULL) {
        return MS_INVALID_PARA;
    }

    if (new_ctx->expr_count == 0 && new_ctx->comb_count == 0) {
        return MS_SUCCESS;
    }

    if (ctx == NULL) {
        *ctx_slot = new_ctx;
        return MS_SUCCESS;
    }

    for (i = 0; i < new_ctx->expr_count; i++) {
        ret = ms_compile_context_add_expr(ctx, new_ctx->exprs[i].id,
                                            new_ctx->exprs[i].flags,
                                            new_ctx->exprs[i].lkey,
                                            new_ctx->exprs[i].pattern_len);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    for (i = 0; i < new_ctx->comb_count; i++) {
        ms_expr_ext_t ext = {0};

        ext.flags = MS_EXT_FLAG_MIN_OFFSET | MS_EXT_FLAG_MAX_OFFSET;
        ext.min_offset = new_ctx->combs[i].min_offset;
        ext.max_offset = new_ctx->combs[i].max_offset;
        ret = ms_compile_context_add_comb(ctx, new_ctx->combs[i].id,
                                            new_ctx->combs[i].flags,
                                            &ext,
                                            &new_ctx->combs[i].tree);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    return MS_SUCCESS;
}

int ms_compile_append_context(ms_handler_t *handler,
                                ms_compile_context_t *new_ctx)
{
    if (handler == NULL) {
        return MS_INVALID_PARA;
    }

    return ms_compile_append_context_impl(
        (ms_compile_context_t **)&handler->ms_compile_ctx, new_ctx);
}

int ms_compile_append_database_context(ms_database_t *db,
                                         ms_compile_context_t *new_ctx)
{
    if (db == NULL) {
        return MS_INVALID_PARA;
    }

    return ms_compile_append_context_impl(
        (ms_compile_context_t **)&db->ms_compile_ctx, new_ctx);
}

void ms_compile_context_free(ms_compile_context_t *ctx)
{
    uint32_t i;

    if (ctx == NULL) {
        return;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        ms_logical_tree_destroy(&ctx->combs[i].tree);
    }

    if (ctx->exprs != NULL) {
        ms_free(ctx->exprs);
    }

    if (ctx->combs != NULL) {
        ms_free(ctx->combs);
    }
    if (ctx->lkey_to_ckeys != NULL) {
        for (i = 0; i < ctx->logical_count; i++) {
            if (ctx->lkey_to_ckeys[i] != NULL) {
                ms_free(ctx->lkey_to_ckeys[i]);
            }
        }
        ms_free(ctx->lkey_to_ckeys);
    }
    if (ctx->lkey_to_ckey_counts != NULL) {
        ms_free(ctx->lkey_to_ckey_counts);
    }
    if (ctx->lkey_to_ckey_capacities != NULL) {
        ms_free(ctx->lkey_to_ckey_capacities);
    }
    if (ctx->lkey_to_expr_indexes != NULL) {
        ms_free(ctx->lkey_to_expr_indexes);
    }
    if (ctx->final_ckeys != NULL) {
        ms_free(ctx->final_ckeys);
    }

    ms_free(ctx);
}

void ms_compile_context_destroy(ms_handler_t *handler)
{
    if (handler == NULL || handler->ms_compile_ctx == NULL) {
        return;
    }

    ms_compile_context_free((ms_compile_context_t *)handler->ms_compile_ctx);
    handler->ms_compile_ctx = NULL;
}

void ms_compile_context_destroy_database(ms_database_t *db)
{
    if (db == NULL || db->ms_compile_ctx == NULL) {
        return;
    }

    ms_compile_context_free((ms_compile_context_t *)db->ms_compile_ctx);
    db->ms_compile_ctx = NULL;
}

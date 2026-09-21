/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ms_allocator.h"
#include "ms_logical.h"

#define MS_LOGICAL_INITIAL_CAPACITY 8u
#define MS_LOGICAL_MAX_ID_DIGITS   9u
#define MS_LOGICAL_UNKNOWN_OP      0xffffffffu
#define MS_LOGICAL_BYTE_BITS       8u

typedef struct {
    uint32_t op;
    uint32_t paren;
} ms_logical_operator_t;

static uint32_t ms_logical_ref_index(uint32_t key)
{
    return key & ~MS_LOGICAL_OP_REF;
}

static int ms_logical_check_key(uint32_t logical_count, uint32_t key)
{
    if (key >= logical_count) {
        return MS_INVALID_PARA;
    }

    return MS_SUCCESS;
}

static uint32_t ms_logical_to_operator(char c)
{
    switch (c) {
        case '!':
            return MS_LOGICAL_OP_NOT;
        case '&':
            return MS_LOGICAL_OP_AND;
        case '|':
            return MS_LOGICAL_OP_OR;
        default:
            return MS_LOGICAL_UNKNOWN_OP;
    }
}

static bool ms_logical_should_pop(const ms_logical_operator_t *top,
                                    const ms_logical_operator_t *cur)
{
    if (top->paren != cur->paren) {
        return top->paren > cur->paren;
    }

    return top->op <= cur->op;
}

static int ms_logical_reserve_ops(ms_logical_tree_t *tree)
{
    ms_logical_op_t *ops = NULL;
    uint32_t new_capacity;

    if (tree->op_count < tree->op_capacity) {
        return MS_SUCCESS;
    }

    if (tree->op_capacity > UINT32_MAX / 2u) {
        return MS_MALLOC_FAIL;
    }
    new_capacity = tree->op_capacity
                       ? tree->op_capacity * 2u
                       : MS_LOGICAL_INITIAL_CAPACITY;
    ops = (ms_logical_op_t *)ms_calloc(new_capacity, sizeof(ms_logical_op_t));
    if (ops == NULL) {
        return MS_MALLOC_FAIL;
    }

    if (tree->ops != NULL) {
        (void)memcpy(ops, tree->ops, tree->op_count * sizeof(ms_logical_op_t));
        ms_free(tree->ops);
    }

    tree->ops = ops;
    tree->op_capacity = new_capacity;
    return MS_SUCCESS;
}

static int ms_logical_add_op(ms_logical_tree_t *tree, uint32_t op,
                               uint32_t left, uint32_t right, uint32_t *out)
{
    int ret = ms_logical_reserve_ops(tree);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    tree->ops[tree->op_count].id = MS_LOGICAL_OP_REF | tree->op_count;
    tree->ops[tree->op_count].op = op;
    tree->ops[tree->op_count].left = left;
    tree->ops[tree->op_count].right = right;
    *out = tree->ops[tree->op_count].id;
    tree->op_count++;
    return MS_SUCCESS;
}

static int ms_logical_fetch_id(const char *logical, uint32_t start,
                                 uint32_t end, uint32_t *id)
{
    uint32_t i;
    uint32_t value = 0;

    if (start == UINT32_MAX) {
        return MS_FIND_FAIL;
    }

    if (end <= start || end - start > MS_LOGICAL_MAX_ID_DIGITS) {
        return MS_INVALID_PARA;
    }

    for (i = start; i < end; i++) {
        value = value * 10 + (uint32_t)(logical[i] - '0');
    }

    *id = value;
    return MS_SUCCESS;
}

static int ms_logical_push_operand(uint32_t **stack, uint32_t *count,
                                     uint32_t *capacity, uint32_t value)
{
    uint32_t *new_stack = NULL;
    uint32_t new_capacity;

    if (*count >= *capacity) {
        if (*capacity > UINT32_MAX / 2u) {
            return MS_MALLOC_FAIL;
        }
        new_capacity = *capacity ? *capacity * 2u : MS_LOGICAL_INITIAL_CAPACITY;
        new_stack = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
        if (new_stack == NULL) {
            return MS_MALLOC_FAIL;
        }
        if (*stack != NULL) {
            (void)memcpy(new_stack, *stack, (*count) * sizeof(uint32_t));
            ms_free(*stack);
        }
        *stack = new_stack;
        *capacity = new_capacity;
    }

    (*stack)[*count] = value;
    (*count)++;
    return MS_SUCCESS;
}

static int ms_logical_push_operator(ms_logical_operator_t **stack,
                                      uint32_t *count, uint32_t *capacity,
                                      ms_logical_operator_t value)
{
    ms_logical_operator_t *new_stack = NULL;
    uint32_t new_capacity;

    if (*count >= *capacity) {
        if (*capacity > UINT32_MAX / 2u) {
            return MS_MALLOC_FAIL;
        }
        new_capacity = *capacity ? *capacity * 2u : MS_LOGICAL_INITIAL_CAPACITY;
        new_stack = (ms_logical_operator_t *)ms_calloc(new_capacity, sizeof(ms_logical_operator_t));
        if (new_stack == NULL) {
            return MS_MALLOC_FAIL;
        }
        if (*stack != NULL) {
            (void)memcpy(new_stack, *stack, (*count) * sizeof(ms_logical_operator_t));
            ms_free(*stack);
        }
        *stack = new_stack;
        *capacity = new_capacity;
    }

    (*stack)[*count] = value;
    (*count)++;
    return MS_SUCCESS;
}

static int ms_logical_pop_operator(ms_logical_tree_t *tree,
                                     ms_logical_operator_t *op_stack,
                                     uint32_t *op_count,
                                     uint32_t *operand_stack,
                                     uint32_t *operand_count)
{
    uint32_t left = 0;
    uint32_t right;
    uint32_t out;
    uint32_t op;

    if (*op_count == 0 || *operand_count == 0) {
        return MS_INVALID_PARA;
    }

    op = op_stack[*op_count - 1].op;
    right = operand_stack[*operand_count - 1];
    (*operand_count)--;

    if (op != MS_LOGICAL_OP_NOT) {
        if (*operand_count == 0) {
            return MS_INVALID_PARA;
        }
        left = operand_stack[*operand_count - 1];
        (*operand_count)--;
    }

    (*op_count)--;
    if (ms_logical_add_op(tree, op, left, right, &out) != MS_SUCCESS) {
        return MS_MALLOC_FAIL;
    }

    operand_stack[*operand_count] = out;
    (*operand_count)++;
    return MS_SUCCESS;
}

static int ms_logical_flush_pending_id(const char *logical, uint32_t *digit_start,
                                         uint32_t pos, uint32_t **operand_stack,
                                         uint32_t *operand_count,
                                         uint32_t *operand_capacity)
{
    uint32_t id;
    int ret;

    ret = ms_logical_fetch_id(logical, *digit_start, pos, &id);
    if (ret == MS_FIND_FAIL) {
        return MS_SUCCESS;
    }
    if (ret != MS_SUCCESS) {
        return ret;
    }

    *digit_start = UINT32_MAX;
    return ms_logical_push_operand(operand_stack, operand_count, operand_capacity, id);
}

__attribute__((visibility("default")))
int ms_logical_parse(const char *logical, ms_logical_tree_t *tree)
{
    ms_logical_operator_t *op_stack = NULL;
    uint32_t *operand_stack = NULL;
    uint32_t op_count = 0;
    uint32_t op_capacity = 0;
    uint32_t operand_count = 0;
    uint32_t operand_capacity = 0;
    uint32_t digit_start = UINT32_MAX;
    uint32_t paren = 0;
    uint32_t i;
    int ret = MS_SUCCESS;

    if (logical == NULL || tree == NULL) {
        return MS_INVALID_PARA;
    }

    (void)memset(tree, 0, sizeof(*tree));
    tree->result = UINT32_MAX;

    for (i = 0; logical[i] != '\0'; i++) {
        unsigned char c = (unsigned char)logical[i];

        if (isdigit(c)) {
            if (digit_start == UINT32_MAX) {
                digit_start = i;
            }
            continue;
        }

        ret = ms_logical_flush_pending_id(logical, &digit_start, i,
                                            &operand_stack, &operand_count,
                                            &operand_capacity);
        if (ret != MS_SUCCESS) {
            goto end;
        }

        if (isspace(c)) {
            continue;
        }

        if (logical[i] == '(') {
            paren++;
            continue;
        }

        if (logical[i] == ')') {
            if (paren == 0) {
                ret = MS_INVALID_PARA;
                goto end;
            }
            paren--;
            continue;
        }

        uint32_t op = ms_logical_to_operator(logical[i]);
        if (op == MS_LOGICAL_UNKNOWN_OP) {
            ret = MS_INVALID_PARA;
            goto end;
        }

        ms_logical_operator_t cur = {op, paren};
        while (op_count > 0 && ms_logical_should_pop(&op_stack[op_count - 1], &cur)) {
            ret = ms_logical_pop_operator(tree, op_stack, &op_count,
                                            operand_stack, &operand_count);
            if (ret != MS_SUCCESS) {
                goto end;
            }
        }

        ret = ms_logical_push_operator(&op_stack, &op_count, &op_capacity, cur);
        if (ret != MS_SUCCESS) {
            goto end;
        }
    }

    ret = ms_logical_flush_pending_id(logical, &digit_start, i,
                                        &operand_stack, &operand_count,
                                        &operand_capacity);
    if (ret != MS_SUCCESS) {
        goto end;
    }

    if (paren != 0) {
        ret = MS_INVALID_PARA;
        goto end;
    }

    while (op_count > 0) {
        ret = ms_logical_pop_operator(tree, op_stack, &op_count,
                                        operand_stack, &operand_count);
        if (ret != MS_SUCCESS) {
            goto end;
        }
    }

    if (operand_count != 1 || tree->op_count == 0) {
        ret = MS_INVALID_PARA;
        goto end;
    }

    tree->result = operand_stack[0];

end:
    if (ret != MS_SUCCESS) {
        ms_logical_tree_destroy(tree);
    }
    if (op_stack != NULL) {
        ms_free(op_stack);
    }
    if (operand_stack != NULL) {
        ms_free(operand_stack);
    }
    return ret;
}

__attribute__((visibility("default")))
void ms_logical_tree_destroy(ms_logical_tree_t *tree)
{
    if (tree == NULL) {
        return;
    }

    if (tree->ops != NULL) {
        ms_free(tree->ops);
    }

    (void)memset(tree, 0, sizeof(*tree));
    tree->result = UINT32_MAX;
}

__attribute__((visibility("default")))
int ms_logical_set(uint8_t *logical_vector, uint32_t logical_count,
                     uint32_t key, uint8_t value)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (logical_vector == NULL) {
        return MS_INVALID_PARA;
    }

    if (ms_logical_check_key(logical_count, key) != MS_SUCCESS) {
        return MS_INVALID_PARA;
    }

    byte_index = key / MS_LOGICAL_BYTE_BITS;
    bit_index = key % MS_LOGICAL_BYTE_BITS;
    if (value) {
        logical_vector[byte_index] |= (uint8_t)(1u << bit_index);
    } else {
        logical_vector[byte_index] &= (uint8_t)(~(1u << bit_index));
    }

    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_logical_get(const uint8_t *logical_vector, uint32_t logical_count,
                     uint32_t key, uint8_t *value)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (logical_vector == NULL || value == NULL) {
        return MS_INVALID_PARA;
    }

    if (ms_logical_check_key(logical_count, key) != MS_SUCCESS) {
        return MS_INVALID_PARA;
    }

    byte_index = key / MS_LOGICAL_BYTE_BITS;
    bit_index = key % MS_LOGICAL_BYTE_BITS;
    *value = (logical_vector[byte_index] & (uint8_t)(1u << bit_index)) ? 1u : 0u;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_logical_clear(uint8_t *logical_vector, uint32_t logical_count)
{
    uint32_t byte_count;

    if (logical_vector == NULL) {
        return MS_INVALID_PARA;
    }

    byte_count = (logical_count + MS_LOGICAL_BYTE_BITS - 1) / MS_LOGICAL_BYTE_BITS;
    (void)memset(logical_vector, 0, byte_count);
    return MS_SUCCESS;
}

static int ms_logical_get_operand(const ms_logical_tree_t *tree,
                                    const uint8_t *logical_vector,
                                    const uint8_t *op_values,
                                    uint32_t logical_count,
                                    uint32_t operand,
                                    uint8_t *value)
{
    uint32_t key = operand;

    if (operand & MS_LOGICAL_OP_REF) {
        uint32_t index = ms_logical_ref_index(operand);
        if (index >= tree->op_count) {
            return MS_INVALID_PARA;
        }
        *value = op_values[index] ? 1u : 0u;
        return MS_SUCCESS;
    }

    return ms_logical_get(logical_vector, logical_count, key, value);
}

__attribute__((visibility("default")))
int ms_logical_eval(const ms_logical_tree_t *tree, uint8_t *logical_vector,
                      uint32_t logical_count, uint8_t *matched)
{
    uint32_t i;
    int ret;
    uint8_t *op_values = NULL;

    if (tree == NULL || logical_vector == NULL || matched == NULL ||
        tree->ops == NULL || tree->op_count == 0) {
        return MS_INVALID_PARA;
    }

    op_values = (uint8_t *)ms_calloc(tree->op_count, sizeof(uint8_t));
    if (op_values == NULL) {
        return MS_MALLOC_FAIL;
    }

    for (i = 0; i < tree->op_count; i++) {
        uint8_t left = 0;
        uint8_t right = 0;
        uint8_t value = 0;
        const ms_logical_op_t *op = &tree->ops[i];

        if (op->op != MS_LOGICAL_OP_NOT) {
            ret = ms_logical_get_operand(tree, logical_vector, op_values,
                                           logical_count, op->left, &left);
            if (ret != MS_SUCCESS) {
                goto end;
            }
        }

        ret = ms_logical_get_operand(tree, logical_vector, op_values,
                                       logical_count, op->right, &right);
        if (ret != MS_SUCCESS) {
            goto end;
        }

        switch ((ms_logical_op_type)op->op) {
            case MS_LOGICAL_OP_NOT:
                value = right ? 0u : 1u;
                break;
            case MS_LOGICAL_OP_AND:
                value = (left && right) ? 1u : 0u;
                break;
            case MS_LOGICAL_OP_OR:
                value = (left || right) ? 1u : 0u;
                break;
            default:
                ret = MS_INVALID_PARA;
                goto end;
        }

        op_values[i] = value;
    }

    if (tree->result & MS_LOGICAL_OP_REF) {
        uint32_t index = ms_logical_ref_index(tree->result);
        if (index >= tree->op_count) {
            ret = MS_INVALID_PARA;
            goto end;
        }
        *matched = op_values[index] ? 1u : 0u;
        ret = MS_SUCCESS;
    } else {
        ret = ms_logical_get(logical_vector, logical_count, tree->result, matched);
    }

end:
    ms_free(op_values);
    return ret;
}

__attribute__((visibility("default")))
int ms_logical_eval_inplace(const ms_logical_tree_t *tree,
                              uint8_t *logical_vector,
                              uint32_t logical_count,
                              uint8_t *matched)
{
    uint32_t i;
    int ret;

    if (tree == NULL || logical_vector == NULL || matched == NULL ||
        tree->ops == NULL || tree->op_count == 0) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < tree->op_count; i++) {
        uint8_t left = 0;
        uint8_t right = 0;
        uint8_t value = 0;
        const ms_logical_op_t *op = &tree->ops[i];

        if ((op->id & MS_LOGICAL_OP_REF) != 0 ||
            (op->left & MS_LOGICAL_OP_REF) != 0 ||
            (op->right & MS_LOGICAL_OP_REF) != 0) {
            return MS_INVALID_PARA;
        }

        if (op->op != MS_LOGICAL_OP_NOT) {
            ret = ms_logical_get(logical_vector, logical_count, op->left, &left);
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }

        ret = ms_logical_get(logical_vector, logical_count, op->right, &right);
        if (ret != MS_SUCCESS) {
            return ret;
        }

        switch ((ms_logical_op_type)op->op) {
            case MS_LOGICAL_OP_NOT:
                value = right ? 0u : 1u;
                break;
            case MS_LOGICAL_OP_AND:
                value = (left && right) ? 1u : 0u;
                break;
            case MS_LOGICAL_OP_OR:
                value = (left || right) ? 1u : 0u;
                break;
            default:
                return MS_INVALID_PARA;
        }

        ret = ms_logical_set(logical_vector, logical_count, op->id, value);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    if ((tree->result & MS_LOGICAL_OP_REF) != 0) {
        return MS_INVALID_PARA;
    }

    return ms_logical_get(logical_vector, logical_count, tree->result, matched);
}

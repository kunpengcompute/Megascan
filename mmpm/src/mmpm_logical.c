/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mmpm_allocator.h"
#include "mmpm_logical.h"

#define MMPM_LOGICAL_INITIAL_CAPACITY 8u
#define MMPM_LOGICAL_MAX_ID_DIGITS   9u
#define MMPM_LOGICAL_UNKNOWN_OP      0xffffffffu
#define MMPM_LOGICAL_BYTE_BITS       8u

typedef struct {
    uint32_t op;
    uint32_t paren;
} mmpm_logical_operator_t;

static uint32_t mmpm_logical_ref_index(uint32_t key)
{
    return key & ~MMPM_LOGICAL_OP_REF;
}

static int mmpm_logical_check_key(uint32_t logical_count, uint32_t key)
{
    if (key >= logical_count) {
        return MMPM_INVALID_PARA;
    }

    return MMPM_SUCCESS;
}

static uint32_t mmpm_logical_to_operator(char c)
{
    switch (c) {
        case '!':
            return MMPM_LOGICAL_OP_NOT;
        case '&':
            return MMPM_LOGICAL_OP_AND;
        case '|':
            return MMPM_LOGICAL_OP_OR;
        default:
            return MMPM_LOGICAL_UNKNOWN_OP;
    }
}

static bool mmpm_logical_should_pop(const mmpm_logical_operator_t *top,
                                    const mmpm_logical_operator_t *cur)
{
    if (top->paren != cur->paren) {
        return top->paren > cur->paren;
    }

    return top->op <= cur->op;
}

static int mmpm_logical_reserve_ops(mmpm_logical_tree_t *tree)
{
    mmpm_logical_op_t *ops = NULL;
    uint32_t new_capacity;

    if (tree->op_count < tree->op_capacity) {
        return MMPM_SUCCESS;
    }

    new_capacity = tree->op_capacity ? tree->op_capacity * 2 : MMPM_LOGICAL_INITIAL_CAPACITY;
    ops = (mmpm_logical_op_t *)mmpm_calloc(new_capacity, sizeof(mmpm_logical_op_t));
    if (ops == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    if (tree->ops != NULL) {
        (void)memcpy(ops, tree->ops, tree->op_count * sizeof(mmpm_logical_op_t));
        mmpm_free(tree->ops);
    }

    tree->ops = ops;
    tree->op_capacity = new_capacity;
    return MMPM_SUCCESS;
}

static int mmpm_logical_add_op(mmpm_logical_tree_t *tree, uint32_t op,
                               uint32_t left, uint32_t right, uint32_t *out)
{
    int ret = mmpm_logical_reserve_ops(tree);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    tree->ops[tree->op_count].id = MMPM_LOGICAL_OP_REF | tree->op_count;
    tree->ops[tree->op_count].op = op;
    tree->ops[tree->op_count].left = left;
    tree->ops[tree->op_count].right = right;
    *out = tree->ops[tree->op_count].id;
    tree->op_count++;
    return MMPM_SUCCESS;
}

static int mmpm_logical_fetch_id(const char *logical, uint32_t start,
                                 uint32_t end, uint32_t *id)
{
    uint32_t i;
    uint32_t value = 0;

    if (start == UINT32_MAX) {
        return MMPM_FIND_FAIL;
    }

    if (end <= start || end - start > MMPM_LOGICAL_MAX_ID_DIGITS) {
        return MMPM_INVALID_PARA;
    }

    for (i = start; i < end; i++) {
        value = value * 10 + (uint32_t)(logical[i] - '0');
    }

    *id = value;
    return MMPM_SUCCESS;
}

static int mmpm_logical_push_operand(uint32_t **stack, uint32_t *count,
                                     uint32_t *capacity, uint32_t value)
{
    uint32_t *new_stack = NULL;
    uint32_t new_capacity;

    if (*count >= *capacity) {
        new_capacity = *capacity ? *capacity * 2 : MMPM_LOGICAL_INITIAL_CAPACITY;
        new_stack = (uint32_t *)mmpm_calloc(new_capacity, sizeof(uint32_t));
        if (new_stack == NULL) {
            return MMPM_MALLOC_FAIL;
        }
        if (*stack != NULL) {
            (void)memcpy(new_stack, *stack, (*count) * sizeof(uint32_t));
            mmpm_free(*stack);
        }
        *stack = new_stack;
        *capacity = new_capacity;
    }

    (*stack)[*count] = value;
    (*count)++;
    return MMPM_SUCCESS;
}

static int mmpm_logical_push_operator(mmpm_logical_operator_t **stack,
                                      uint32_t *count, uint32_t *capacity,
                                      mmpm_logical_operator_t value)
{
    mmpm_logical_operator_t *new_stack = NULL;
    uint32_t new_capacity;

    if (*count >= *capacity) {
        new_capacity = *capacity ? *capacity * 2 : MMPM_LOGICAL_INITIAL_CAPACITY;
        new_stack = (mmpm_logical_operator_t *)mmpm_calloc(new_capacity, sizeof(mmpm_logical_operator_t));
        if (new_stack == NULL) {
            return MMPM_MALLOC_FAIL;
        }
        if (*stack != NULL) {
            (void)memcpy(new_stack, *stack, (*count) * sizeof(mmpm_logical_operator_t));
            mmpm_free(*stack);
        }
        *stack = new_stack;
        *capacity = new_capacity;
    }

    (*stack)[*count] = value;
    (*count)++;
    return MMPM_SUCCESS;
}

static int mmpm_logical_pop_operator(mmpm_logical_tree_t *tree,
                                     mmpm_logical_operator_t *op_stack,
                                     uint32_t *op_count,
                                     uint32_t *operand_stack,
                                     uint32_t *operand_count)
{
    uint32_t left = 0;
    uint32_t right;
    uint32_t out;
    uint32_t op;

    if (*op_count == 0 || *operand_count == 0) {
        return MMPM_INVALID_PARA;
    }

    op = op_stack[*op_count - 1].op;
    right = operand_stack[*operand_count - 1];
    (*operand_count)--;

    if (op != MMPM_LOGICAL_OP_NOT) {
        if (*operand_count == 0) {
            return MMPM_INVALID_PARA;
        }
        left = operand_stack[*operand_count - 1];
        (*operand_count)--;
    }

    (*op_count)--;
    if (mmpm_logical_add_op(tree, op, left, right, &out) != MMPM_SUCCESS) {
        return MMPM_MALLOC_FAIL;
    }

    operand_stack[*operand_count] = out;
    (*operand_count)++;
    return MMPM_SUCCESS;
}

static int mmpm_logical_flush_pending_id(const char *logical, uint32_t *digit_start,
                                         uint32_t pos, uint32_t **operand_stack,
                                         uint32_t *operand_count,
                                         uint32_t *operand_capacity)
{
    uint32_t id;
    int ret;

    ret = mmpm_logical_fetch_id(logical, *digit_start, pos, &id);
    if (ret == MMPM_FIND_FAIL) {
        return MMPM_SUCCESS;
    }
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    *digit_start = UINT32_MAX;
    return mmpm_logical_push_operand(operand_stack, operand_count, operand_capacity, id);
}

__attribute__((visibility("default")))
int mmpm_logical_parse(const char *logical, mmpm_logical_tree_t *tree)
{
    mmpm_logical_operator_t *op_stack = NULL;
    uint32_t *operand_stack = NULL;
    uint32_t op_count = 0;
    uint32_t op_capacity = 0;
    uint32_t operand_count = 0;
    uint32_t operand_capacity = 0;
    uint32_t digit_start = UINT32_MAX;
    uint32_t paren = 0;
    uint32_t i;
    int ret = MMPM_SUCCESS;

    if (logical == NULL || tree == NULL) {
        return MMPM_INVALID_PARA;
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

        ret = mmpm_logical_flush_pending_id(logical, &digit_start, i,
                                            &operand_stack, &operand_count,
                                            &operand_capacity);
        if (ret != MMPM_SUCCESS) {
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
                ret = MMPM_INVALID_PARA;
                goto end;
            }
            paren--;
            continue;
        }

        uint32_t op = mmpm_logical_to_operator(logical[i]);
        if (op == MMPM_LOGICAL_UNKNOWN_OP) {
            ret = MMPM_INVALID_PARA;
            goto end;
        }

        mmpm_logical_operator_t cur = {op, paren};
        while (op_count > 0 && mmpm_logical_should_pop(&op_stack[op_count - 1], &cur)) {
            ret = mmpm_logical_pop_operator(tree, op_stack, &op_count,
                                            operand_stack, &operand_count);
            if (ret != MMPM_SUCCESS) {
                goto end;
            }
        }

        ret = mmpm_logical_push_operator(&op_stack, &op_count, &op_capacity, cur);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
    }

    ret = mmpm_logical_flush_pending_id(logical, &digit_start, i,
                                        &operand_stack, &operand_count,
                                        &operand_capacity);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    if (paren != 0) {
        ret = MMPM_INVALID_PARA;
        goto end;
    }

    while (op_count > 0) {
        ret = mmpm_logical_pop_operator(tree, op_stack, &op_count,
                                        operand_stack, &operand_count);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
    }

    if (operand_count != 1 || tree->op_count == 0) {
        ret = MMPM_INVALID_PARA;
        goto end;
    }

    tree->result = operand_stack[0];

end:
    if (ret != MMPM_SUCCESS) {
        mmpm_logical_tree_destroy(tree);
    }
    if (op_stack != NULL) {
        mmpm_free(op_stack);
    }
    if (operand_stack != NULL) {
        mmpm_free(operand_stack);
    }
    return ret;
}

__attribute__((visibility("default")))
void mmpm_logical_tree_destroy(mmpm_logical_tree_t *tree)
{
    if (tree == NULL) {
        return;
    }

    if (tree->ops != NULL) {
        mmpm_free(tree->ops);
    }

    (void)memset(tree, 0, sizeof(*tree));
    tree->result = UINT32_MAX;
}

__attribute__((visibility("default")))
int mmpm_logical_set(uint8_t *logical_vector, uint32_t logical_count,
                     uint32_t key, uint8_t value)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (logical_vector == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (mmpm_logical_check_key(logical_count, key) != MMPM_SUCCESS) {
        return MMPM_INVALID_PARA;
    }

    byte_index = key / MMPM_LOGICAL_BYTE_BITS;
    bit_index = key % MMPM_LOGICAL_BYTE_BITS;
    if (value) {
        logical_vector[byte_index] |= (uint8_t)(1u << bit_index);
    } else {
        logical_vector[byte_index] &= (uint8_t)(~(1u << bit_index));
    }

    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_logical_get(const uint8_t *logical_vector, uint32_t logical_count,
                     uint32_t key, uint8_t *value)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (logical_vector == NULL || value == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (mmpm_logical_check_key(logical_count, key) != MMPM_SUCCESS) {
        return MMPM_INVALID_PARA;
    }

    byte_index = key / MMPM_LOGICAL_BYTE_BITS;
    bit_index = key % MMPM_LOGICAL_BYTE_BITS;
    *value = (logical_vector[byte_index] & (uint8_t)(1u << bit_index)) ? 1u : 0u;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_logical_clear(uint8_t *logical_vector, uint32_t logical_count)
{
    uint32_t byte_count;

    if (logical_vector == NULL) {
        return MMPM_INVALID_PARA;
    }

    byte_count = (logical_count + MMPM_LOGICAL_BYTE_BITS - 1) / MMPM_LOGICAL_BYTE_BITS;
    (void)memset(logical_vector, 0, byte_count);
    return MMPM_SUCCESS;
}

static int mmpm_logical_get_operand(const mmpm_logical_tree_t *tree,
                                    const uint8_t *logical_vector,
                                    const uint8_t *op_values,
                                    uint32_t logical_count,
                                    uint32_t operand,
                                    uint8_t *value)
{
    uint32_t key = operand;

    if (operand & MMPM_LOGICAL_OP_REF) {
        uint32_t index = mmpm_logical_ref_index(operand);
        if (index >= tree->op_count) {
            return MMPM_INVALID_PARA;
        }
        *value = op_values[index] ? 1u : 0u;
        return MMPM_SUCCESS;
    }

    return mmpm_logical_get(logical_vector, logical_count, key, value);
}

__attribute__((visibility("default")))
int mmpm_logical_eval(const mmpm_logical_tree_t *tree, uint8_t *logical_vector,
                      uint32_t logical_count, uint8_t *matched)
{
    uint32_t i;
    int ret;
    uint8_t *op_values = NULL;

    if (tree == NULL || logical_vector == NULL || matched == NULL ||
        tree->ops == NULL || tree->op_count == 0) {
        return MMPM_INVALID_PARA;
    }

    op_values = (uint8_t *)mmpm_calloc(tree->op_count, sizeof(uint8_t));
    if (op_values == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    for (i = 0; i < tree->op_count; i++) {
        uint8_t left = 0;
        uint8_t right = 0;
        uint8_t value = 0;
        const mmpm_logical_op_t *op = &tree->ops[i];

        if (op->op != MMPM_LOGICAL_OP_NOT) {
            ret = mmpm_logical_get_operand(tree, logical_vector, op_values,
                                           logical_count, op->left, &left);
            if (ret != MMPM_SUCCESS) {
                goto end;
            }
        }

        ret = mmpm_logical_get_operand(tree, logical_vector, op_values,
                                       logical_count, op->right, &right);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }

        switch ((mmpm_logical_op_type)op->op) {
            case MMPM_LOGICAL_OP_NOT:
                value = right ? 0u : 1u;
                break;
            case MMPM_LOGICAL_OP_AND:
                value = (left && right) ? 1u : 0u;
                break;
            case MMPM_LOGICAL_OP_OR:
                value = (left || right) ? 1u : 0u;
                break;
            default:
                ret = MMPM_INVALID_PARA;
                goto end;
        }

        op_values[i] = value;
    }

    if (tree->result & MMPM_LOGICAL_OP_REF) {
        uint32_t index = mmpm_logical_ref_index(tree->result);
        if (index >= tree->op_count) {
            ret = MMPM_INVALID_PARA;
            goto end;
        }
        *matched = op_values[index] ? 1u : 0u;
        ret = MMPM_SUCCESS;
    } else {
        ret = mmpm_logical_get(logical_vector, logical_count, tree->result, matched);
    }

end:
    mmpm_free(op_values);
    return ret;
}

__attribute__((visibility("default")))
int mmpm_logical_eval_inplace(const mmpm_logical_tree_t *tree,
                              uint8_t *logical_vector,
                              uint32_t logical_count,
                              uint8_t *matched)
{
    uint32_t i;
    int ret;

    if (tree == NULL || logical_vector == NULL || matched == NULL ||
        tree->ops == NULL || tree->op_count == 0) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < tree->op_count; i++) {
        uint8_t left = 0;
        uint8_t right = 0;
        uint8_t value = 0;
        const mmpm_logical_op_t *op = &tree->ops[i];

        if ((op->id & MMPM_LOGICAL_OP_REF) != 0 ||
            (op->left & MMPM_LOGICAL_OP_REF) != 0 ||
            (op->right & MMPM_LOGICAL_OP_REF) != 0) {
            return MMPM_INVALID_PARA;
        }

        if (op->op != MMPM_LOGICAL_OP_NOT) {
            ret = mmpm_logical_get(logical_vector, logical_count, op->left, &left);
            if (ret != MMPM_SUCCESS) {
                return ret;
            }
        }

        ret = mmpm_logical_get(logical_vector, logical_count, op->right, &right);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }

        switch ((mmpm_logical_op_type)op->op) {
            case MMPM_LOGICAL_OP_NOT:
                value = right ? 0u : 1u;
                break;
            case MMPM_LOGICAL_OP_AND:
                value = (left && right) ? 1u : 0u;
                break;
            case MMPM_LOGICAL_OP_OR:
                value = (left || right) ? 1u : 0u;
                break;
            default:
                return MMPM_INVALID_PARA;
        }

        ret = mmpm_logical_set(logical_vector, logical_count, op->id, value);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    if ((tree->result & MMPM_LOGICAL_OP_REF) != 0) {
        return MMPM_INVALID_PARA;
    }

    return mmpm_logical_get(logical_vector, logical_count, tree->result, matched);
}

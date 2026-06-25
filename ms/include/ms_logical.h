/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_LOGICAL_H
#define MS_LOGICAL_H

#include <stdint.h>

#include "ms_common.h"

#define MS_LOGICAL_OP_REF 0x80000000u

typedef enum {
    MS_LOGICAL_OP_NOT = 0,
    MS_LOGICAL_OP_AND,
    MS_LOGICAL_OP_OR
} ms_logical_op_type;

typedef struct {
    uint32_t id;
    uint32_t op;
    uint32_t left;
    uint32_t right;
} ms_logical_op_t;

typedef struct {
    ms_logical_op_t *ops;
    uint32_t op_count;
    uint32_t op_capacity;
    uint32_t result;
} ms_logical_tree_t;

int ms_logical_parse(const char *logical, ms_logical_tree_t *tree);
void ms_logical_tree_destroy(ms_logical_tree_t *tree);
int ms_logical_set(uint8_t *logical_vector, uint32_t logical_count,
                     uint32_t key, uint8_t value);
int ms_logical_get(const uint8_t *logical_vector, uint32_t logical_count,
                     uint32_t key, uint8_t *value);
int ms_logical_clear(uint8_t *logical_vector, uint32_t logical_count);
int ms_logical_eval(const ms_logical_tree_t *tree, uint8_t *logical_vector,
                      uint32_t logical_count, uint8_t *matched);
int ms_logical_eval_inplace(const ms_logical_tree_t *tree,
                              uint8_t *logical_vector,
                              uint32_t logical_count,
                              uint8_t *matched);

#endif

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_LOGICAL_H
#define MMPM_LOGICAL_H

#include <stdint.h>
#include "mmpm.h"

#define MMPM_LOGICAL_OP_REF 0x80000000u

typedef enum {
    MMPM_LOGICAL_OP_NOT = 0,
    MMPM_LOGICAL_OP_AND,
    MMPM_LOGICAL_OP_OR
} mmpm_logical_op_type;

typedef struct {
    uint32_t id;
    uint32_t op;
    uint32_t left;
    uint32_t right;
} mmpm_logical_op_t;

typedef struct {
    mmpm_logical_op_t *ops;
    uint32_t op_count;
    uint32_t op_capacity;
    uint32_t result;
} mmpm_logical_tree_t;

int mmpm_logical_parse(const char *logical, mmpm_logical_tree_t *tree);
void mmpm_logical_tree_destroy(mmpm_logical_tree_t *tree);
int mmpm_logical_set(uint8_t *logical_vector, uint32_t logical_count,
                     uint32_t key, uint8_t value);
int mmpm_logical_get(const uint8_t *logical_vector, uint32_t logical_count,
                     uint32_t key, uint8_t *value);
int mmpm_logical_clear(uint8_t *logical_vector, uint32_t logical_count);
int mmpm_logical_eval(const mmpm_logical_tree_t *tree, uint8_t *logical_vector,
                      uint32_t logical_count, uint8_t *matched);
int mmpm_logical_eval_inplace(const mmpm_logical_tree_t *tree,
                              uint8_t *logical_vector,
                              uint32_t logical_count,
                              uint8_t *matched);

#endif

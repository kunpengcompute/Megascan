/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_AC_H
#define MMPM_AC_H

#include <stdbool.h>
#include "mmpm.h"

#define BYTE_MAX_SIZE 256  // number of state transition conditions in bytes

typedef struct ACNode {
    struct ACNode* children[BYTE_MAX_SIZE];
    struct ACNode* fail;
    int* id_set;
    int* output;
    int id_size;
    int out_size;
    bool flags[BYTE_MAX_SIZE];              // marks whether the pointer is an ancestor or a child node.
    uint32_t node_idx;
} ACNode;

typedef struct {
    ACNode* root;
    uint32_t pattern_count;
    uint32_t id_count;
    uint32_t *table;
    int **output;
} ACAutomaton;

int ac_automaton_init(ACAutomaton *automaton);
int ac_automaton_destroy(ACAutomaton *automaton);
int ac_automaton_insert(ACAutomaton* automaton,  uint32_t alias, const uint8_t *pattern, uint32_t len);
int ac_automaton_build(ACAutomaton* automaton);
int ac_automaton_match(ACAutomaton *automaton, const uint8_t *data, uint32_t len);

#endif
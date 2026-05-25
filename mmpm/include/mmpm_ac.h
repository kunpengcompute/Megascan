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
    bool is_non_child_node[BYTE_MAX_SIZE];  // false: child node pointer; true: non-child pointer (ancestor/fail/...).
    uint32_t node_idx;
    struct ACNode* parent;      
    uint8_t parent_ch;          
} ACNode;

typedef struct {
    ACNode* root;
    uint32_t pattern_count;
    uint32_t id_count;
    uint32_t *table;
    int **output;
    uint64_t generation;
} ACAutomaton;

int ac_automaton_init(ACAutomaton *automaton);
int ac_automaton_destroy(ACAutomaton *automaton);
int ac_automaton_insert(ACAutomaton* automaton,  uint32_t alias, const uint8_t *pattern, uint32_t len);
int ac_automaton_build(ACAutomaton* automaton);
int ac_automaton_match(ACAutomaton *automaton, const uint8_t *data, uint32_t len);
int ac_automaton_remove(ACAutomaton *automaton, uint32_t alias, const uint8_t *pattern, uint32_t len);
int mmpm_open_stream(mmpm_handler_t *handler, ms_stream_t **stream);
int mmpm_scan_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len);
int mmpm_reset_stream(ms_stream_t *stream);
int mmpm_close_stream(ms_stream_t *stream);
int ac_automaton_match_state(ACAutomaton *automaton, const uint8_t *data, uint32_t len, uint32_t *node_index);

#endif

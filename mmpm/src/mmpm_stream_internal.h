/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_STREAM_INTERNAL_H
#define MMPM_STREAM_INTERNAL_H

#include "mmpm_database_internal.h"
#include "mmpm_ac.h"

struct ms_stream {
    const mmpm_database_t *database;
    mmpm_handler_t *legacy_handler;
    mmpm_handler_t handler_view;
    ACAutomaton *automaton;
    uint32_t node_index;
    uint64_t offset;
    uint64_t generation;
    uint8_t *logical_vector;
    uint32_t logical_count;
    uint8_t *combination_vector;
    uint32_t combination_count;
    uint8_t *exhaustion_vector;
    uint32_t exhaustion_count;
};

#endif

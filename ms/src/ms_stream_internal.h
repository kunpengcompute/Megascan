/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_STREAM_INTERNAL_H
#define MS_STREAM_INTERNAL_H

#include "ms_database_internal.h"
#include "ms_ac.h"
#include "ms_acdb.h"

typedef struct {
    uint8_t *marked;
    uint32_t byte_count;
    uint32_t *indexes;
    uint32_t count;
    uint32_t capacity;
} ms_stream_dirty_bytes_t;

struct ms_stream {
    const ms_database_t *database;
    ms_handler_t *legacy_handler;
    ms_database_t database_view;
    ACAutomaton *automaton;
    const ms_acdb_t *acdb;
    uint32_t node_index;
    uint64_t offset;
    uint64_t generation;
    uint8_t *logical_vector;
    uint32_t logical_count;
    uint8_t *combination_vector;
    uint32_t combination_count;
    uint8_t *exhaustion_vector;
    uint32_t exhaustion_count;
    ms_stream_dirty_bytes_t logical_dirty;
    ms_stream_dirty_bytes_t combination_dirty;
    ms_stream_dirty_bytes_t exhaustion_dirty;
};

#endif

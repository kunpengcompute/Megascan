/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>

#include "mmpm.h"
#include "mmpm_ac.h"
#include "mmpm_allocator.h"
#include "mmpm_rule_internal.h"
#include "expr/mmpm_expr_internal.h"

struct ms_stream {
    mmpm_handler_t *handler;
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

__attribute__((visibility("default")))
int mmpm_open_stream(mmpm_handler_t *handler, ms_stream_t **stream)
{
    int ret;
    ACAutomaton *automaton;
    ms_stream_t *s = NULL;

    if (handler == NULL || stream == NULL || *stream != NULL) {
        return MMPM_INVALID_PARA;
    }

    if (handler->type != MMPM_MATCHER_TYPE_AC ||
        handler->mmpm_matcher == NULL ||
        !handler->is_built) {
        return MMPM_INVALID_PARA;
    }

    s = (ms_stream_t *)mmpm_calloc(1, sizeof(ms_stream_t));
    if (s == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    automaton = (ACAutomaton *)handler->mmpm_matcher;
    s->handler = handler;
    s->automaton = automaton;
    s->node_index = 0;
    s->offset = 0;
    s->generation = automaton->generation;
    ret = mmpm_compile_stream_state_init(handler,
                                         &s->logical_vector,
                                         &s->logical_count,
                                         &s->combination_vector,
                                         &s->combination_count,
                                         &s->exhaustion_vector,
                                         &s->exhaustion_count);
    if (ret != MMPM_SUCCESS) {
        mmpm_free(s);
        return ret;
    }

    *stream = s;
    return MMPM_SUCCESS;
}

int mmpm_stream_scan_direct(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    int ret;

    if (stream == NULL || stream->automaton == NULL || data == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MMPM_STREAM_STALE;
    }

    ret = ac_automaton_match_state(stream->automaton,
                                   data,
                                   len,
                                   &stream->node_index);
    if (ret == MMPM_SUCCESS || ret == MMPM_FIND_FAIL) {
        stream->offset += len;
    }

    return ret;
}

__attribute__((visibility("default")))
int mmpm_scan_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    mmpm_compile_context_t *ctx;
    uint32_t *ids = NULL;
    uint32_t id_count = 0;
    uint32_t id_capacity;
    int ret;

    if (stream == NULL || stream->automaton == NULL || data == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MMPM_STREAM_STALE;
    }

    if (stream->handler == NULL || stream->handler->mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    ctx = (mmpm_compile_context_t *)stream->handler->mmpm_compile_ctx;
    id_capacity = ctx->expr_count + ctx->comb_count;
    if (id_capacity != 0) {
        ids = (uint32_t *)mmpm_calloc(id_capacity, sizeof(uint32_t));
        if (ids == NULL) {
            return MMPM_MALLOC_FAIL;
        }
    }

    ret = mmpm_scan_expr_stream(stream, data, len, ids, &id_count, id_capacity);
    if (ids != NULL) {
        mmpm_free(ids);
    }
    return ret;
}

__attribute__((visibility("default")))
int mmpm_scan_expr_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len,
                          uint32_t *ids, uint32_t *id_count, uint32_t id_capacity)
{
    if (stream == NULL || stream->automaton == NULL || stream->handler == NULL ||
        data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MMPM_STREAM_STALE;
    }

    if (stream->handler->mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    return mmpm_expr_scan_stream(stream->handler,
                                 stream->automaton,
                                 &stream->node_index,
                                 stream->logical_vector,
                                 stream->logical_count,
                                 stream->combination_vector,
                                 stream->combination_count,
                                 stream->exhaustion_vector,
                                 stream->exhaustion_count,
                                 &stream->offset,
                                 data,
                                 len,
                                 ids,
                                 id_count,
                                 id_capacity);
}

__attribute__((visibility("default")))
int mmpm_scan_expr_stream_flush(ms_stream_t *stream,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity)
{
    if (stream == NULL || stream->automaton == NULL || stream->handler == NULL ||
        id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MMPM_STREAM_STALE;
    }

    return mmpm_expr_flush_stream(stream->handler,
                                  stream->logical_vector,
                                  stream->logical_count,
                                  stream->combination_vector,
                                  stream->combination_count,
                                  stream->exhaustion_vector,
                                  stream->exhaustion_count,
                                  stream->offset,
                                  ids,
                                  id_count,
                                  id_capacity);
}

__attribute__((visibility("default")))
int mmpm_reset_stream(ms_stream_t *stream)
{
    int ret;

    if (stream == NULL || stream->automaton == NULL) {
        return MMPM_INVALID_PARA;
    }

    stream->node_index = 0;
    stream->offset = 0;
    stream->generation = stream->automaton->generation;

    mmpm_compile_stream_state_destroy(stream->logical_vector,
                                      stream->combination_vector,
                                      stream->exhaustion_vector);
    stream->logical_vector = NULL;
    stream->logical_count = 0;
    stream->combination_vector = NULL;
    stream->combination_count = 0;
    stream->exhaustion_vector = NULL;
    stream->exhaustion_count = 0;
    ret = mmpm_compile_stream_state_init(stream->handler,
                                         &stream->logical_vector,
                                         &stream->logical_count,
                                         &stream->combination_vector,
                                         &stream->combination_count,
                                         &stream->exhaustion_vector,
                                         &stream->exhaustion_count);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_close_stream(ms_stream_t *stream)
{
    if (stream == NULL) {
        return MMPM_INVALID_PARA;
    }

    mmpm_compile_stream_state_destroy(stream->logical_vector,
                                      stream->combination_vector,
                                      stream->exhaustion_vector);
    mmpm_free(stream);
    return MMPM_SUCCESS;
}

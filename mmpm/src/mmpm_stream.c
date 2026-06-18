/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>
#include <string.h>

#include "mmpm.h"
#include "mmpm_ac.h"
#include "mmpm_allocator.h"
#include "mmpm_rule_internal.h"
#include "mmpm_stream_internal.h"
#include "expr/mmpm_expr_internal.h"

static int mmpm_open_stream_view(const mmpm_database_t *database,
                                 mmpm_handler_t *legacy_handler,
                                 const mmpm_handler_t *handler,
                                 ms_stream_t **stream)
{
    int ret;
    ACAutomaton *automaton;
    ms_stream_t *s = NULL;

    if (handler == NULL || stream == NULL || *stream != NULL) {
        return MMPM_INVALID_PARA;
    }
    if (handler->type != MMPM_MATCHER_TYPE_AC ||
        handler->mmpm_matcher == NULL || !handler->is_built) {
        return MMPM_INVALID_PARA;
    }

    s = (ms_stream_t *)mmpm_stream_calloc(1, sizeof(ms_stream_t));
    if (s == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    automaton = (ACAutomaton *)handler->mmpm_matcher;
    s->database = database;
    s->legacy_handler = legacy_handler;
    s->handler_view = *handler;
    s->automaton = automaton;
    s->generation = automaton->generation;
    ret = mmpm_compile_stream_state_init(&s->handler_view,
                                         &s->logical_vector,
                                         &s->logical_count,
                                         &s->combination_vector,
                                         &s->combination_count,
                                         &s->exhaustion_vector,
                                         &s->exhaustion_count);
    if (ret != MMPM_SUCCESS) {
        mmpm_stream_free(s);
        return ret;
    }

    *stream = s;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_open_stream(mmpm_handler_t *handler, ms_stream_t **stream)
{
    return mmpm_open_stream_view(NULL, handler, handler, stream);
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_open_database_stream(const mmpm_database_t *db, ms_stream_t **stream)
{
    mmpm_handler_t handler = {0};

    if (db == NULL || db->type != MMPM_MATCHER_TYPE_AC ||
        db->mmpm_matcher == NULL || !db->is_built ||
        db->mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    mmpm_database_to_handler(db, &handler);
    return mmpm_open_stream_view(db, NULL, &handler, stream);
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

    if (stream->handler_view.mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    ctx = (mmpm_compile_context_t *)stream->handler_view.mmpm_compile_ctx;
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
    if (stream == NULL || stream->automaton == NULL ||
        data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MMPM_STREAM_STALE;
    }

    if (stream->handler_view.mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    return mmpm_expr_scan_stream(&stream->handler_view,
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
    if (stream == NULL || stream->automaton == NULL ||
        id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MMPM_STREAM_STALE;
    }

    return mmpm_expr_flush_stream(&stream->handler_view,
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
    uint32_t bytes;

    if (stream == NULL || stream->automaton == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (stream->legacy_handler != NULL) {
        if (stream->legacy_handler->type != MMPM_MATCHER_TYPE_AC ||
            stream->legacy_handler->mmpm_matcher == NULL ||
            !stream->legacy_handler->is_built) {
            return MMPM_INVALID_PARA;
        }

        mmpm_compile_stream_state_destroy(stream->logical_vector,
                                          stream->combination_vector,
                                          stream->exhaustion_vector);
        stream->logical_vector = NULL;
        stream->logical_count = 0;
        stream->combination_vector = NULL;
        stream->combination_count = 0;
        stream->exhaustion_vector = NULL;
        stream->exhaustion_count = 0;
        stream->handler_view = *stream->legacy_handler;
        stream->automaton = (ACAutomaton *)stream->handler_view.mmpm_matcher;
        ret = mmpm_compile_stream_state_init(&stream->handler_view,
                                             &stream->logical_vector,
                                             &stream->logical_count,
                                             &stream->combination_vector,
                                             &stream->combination_count,
                                             &stream->exhaustion_vector,
                                             &stream->exhaustion_count);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    } else if (stream->database == NULL) {
        return MMPM_INVALID_PARA;
    }

    stream->node_index = 0;
    stream->offset = 0;
    stream->generation = stream->automaton->generation;

    bytes = mmpm_compile_logical_byte_count(stream->logical_count);
    if (bytes != 0) {
        (void)memset(stream->logical_vector, 0, bytes);
    }
    bytes = mmpm_compile_logical_byte_count(stream->combination_count);
    if (bytes != 0) {
        (void)memset(stream->combination_vector, 0, bytes);
    }
    bytes = mmpm_compile_logical_byte_count(stream->exhaustion_count);
    if (bytes != 0) {
        (void)memset(stream->exhaustion_vector, 0, bytes);
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
    mmpm_stream_free(stream);
    return MMPM_SUCCESS;
}

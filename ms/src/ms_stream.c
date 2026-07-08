/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>
#include <string.h>

#include "ms.h"
#include "ms_ac.h"
#include "ms_allocator.h"
#include "ms_runtime.h"
#include "ms_rule_internal.h"
#include "ms_stream_internal.h"
#include "expr/ms_expr_internal.h"

static int ms_open_stream_view(const ms_database_t *database,
                                 ms_handler_t *legacy_handler,
                                 const ms_database_t *db_view,
                                 ms_stream_t **stream)
{
    int ret;
    ACAutomaton *automaton;
    ms_stream_t *s = NULL;

    if (db_view == NULL || stream == NULL || *stream != NULL) {
        return MS_INVALID_PARA;
    }
    if (db_view->type != MS_MATCHER_TYPE_AC ||
        db_view->ms_matcher == NULL || !db_view->is_built) {
        return MS_INVALID_PARA;
    }

    s = (ms_stream_t *)ms_stream_calloc(1, sizeof(ms_stream_t));
    if (s == NULL) {
        return MS_MALLOC_FAIL;
    }

    automaton = (ACAutomaton *)db_view->ms_matcher;
    s->database = database;
    s->legacy_handler = legacy_handler;
    s->database_view = *db_view;
    s->automaton = automaton;
    s->generation = automaton->generation;
    ret = ms_compile_stream_state_init(&s->database_view,
                                         &s->logical_vector,
                                         &s->logical_count,
                                         &s->combination_vector,
                                         &s->combination_count,
                                         &s->exhaustion_vector,
                                         &s->exhaustion_count);
    if (ret != MS_SUCCESS) {
        ms_stream_free(s);
        return ret;
    }

    *stream = s;
    return MS_SUCCESS;
}

static const ms_database_t *ms_stream_expected_database(const ms_stream_t *stream)
{
    if (stream == NULL) {
        return NULL;
    }

    return (stream->database != NULL) ? stream->database : &stream->database_view;
}

static int ms_stream_call_with_temporary_scratch(ms_stream_t *stream,
                                                   const uint8_t *data,
                                                   uint32_t len,
                                                   uint32_t *ids,
                                                   uint32_t *id_count,
                                                   uint32_t id_capacity,
                                                   bool flush_only)
{
    const ms_database_t *db;
    ms_scratch_t *scratch = NULL;
    int ret;
    int free_ret;

    if (stream == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }
    if (!flush_only && data == NULL) {
        return MS_INVALID_PARA;
    }

    db = ms_stream_expected_database(stream);
    if (db == NULL) {
        return MS_INVALID_PARA;
    }

    ret = ms_alloc_scratch(db, &scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    if (flush_only) {
        ret = ms_flush_stream(stream, scratch, ids, id_count, id_capacity);
    } else {
        ret = ms_scan_stream(stream, data, len, scratch, ids, id_count, id_capacity);
    }
    free_ret = ms_free_scratch(scratch);
    if ((ret == MS_SUCCESS || ret == MS_FIND_FAIL) && free_ret != MS_SUCCESS) {
        return free_ret;
    }

    return ret;
}

__attribute__((visibility("default")))
int ms_open_handler_stream(ms_handler_t *handler, ms_stream_t **stream)
{
    ms_database_t db_view = {0};

    if (handler == NULL) {
        return MS_INVALID_PARA;
    }

    ms_database_from_handler(&db_view, handler);
    return ms_open_stream_view(NULL, handler, &db_view, stream);
}

__attribute__((visibility("default")))
int MS_CDECL ms_open_stream(const ms_database_t *db, ms_stream_t **stream)
{
    if (db == NULL || db->type != MS_MATCHER_TYPE_AC ||
        db->ms_matcher == NULL || !db->is_built ||
        db->ms_compile_ctx == NULL) {
        return MS_INVALID_PARA;
    }

    return ms_open_stream_view(db, NULL, db, stream);
}

int ms_stream_scan_direct(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    int ret;

    if (stream == NULL || stream->automaton == NULL || data == NULL) {
        return MS_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MS_STREAM_STALE;
    }

    ret = ac_automaton_match_state(stream->automaton,
                                   data,
                                   len,
                                   &stream->node_index);
    if (ret == MS_SUCCESS || ret == MS_FIND_FAIL) {
        stream->offset += len;
    }

    return ret;
}

__attribute__((visibility("default")))
int ms_scan_rule_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    ms_compile_context_t *ctx;
    uint32_t *ids = NULL;
    uint32_t id_count = 0;
    uint32_t id_capacity;
    int ret;

    if (stream == NULL || stream->automaton == NULL || data == NULL) {
        return MS_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MS_STREAM_STALE;
    }

    if (stream->database_view.ms_compile_ctx == NULL) {
        return MS_INVALID_PARA;
    }

    ctx = (ms_compile_context_t *)stream->database_view.ms_compile_ctx;
    id_capacity = ctx->expr_count + ctx->comb_count;
    if (id_capacity != 0) {
        ids = (uint32_t *)ms_calloc(id_capacity, sizeof(uint32_t));
        if (ids == NULL) {
            return MS_MALLOC_FAIL;
        }
    }

    ret = ms_scan_expr_stream(stream, data, len, ids, &id_count, id_capacity);
    if (ids != NULL) {
        ms_free(ids);
    }
    return ret;
}

__attribute__((visibility("default")))
int ms_scan_expr_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len,
                          uint32_t *ids, uint32_t *id_count, uint32_t id_capacity)
{
    if (stream == NULL || stream->automaton == NULL ||
        data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MS_STREAM_STALE;
    }

    if (stream->database_view.ms_compile_ctx == NULL) {
        return MS_INVALID_PARA;
    }

    return ms_stream_call_with_temporary_scratch(stream,
                                                   data,
                                                   len,
                                                   ids,
                                                   id_count,
                                                   id_capacity,
                                                   false);
}

__attribute__((visibility("default")))
int ms_scan_expr_stream_flush(ms_stream_t *stream,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity)
{
    if (stream == NULL || stream->automaton == NULL ||
        id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MS_STREAM_STALE;
    }

    return ms_stream_call_with_temporary_scratch(stream,
                                                   NULL,
                                                   0,
                                                   ids,
                                                   id_count,
                                                   id_capacity,
                                                   true);
}

__attribute__((visibility("default")))
int ms_reset_stream(ms_stream_t *stream)
{
    int ret;
    uint32_t bytes;

    if (stream == NULL || stream->automaton == NULL) {
        return MS_INVALID_PARA;
    }

    if (stream->legacy_handler != NULL) {
        if (stream->legacy_handler->type != MS_MATCHER_TYPE_AC ||
            stream->legacy_handler->ms_matcher == NULL ||
            !stream->legacy_handler->is_built) {
            return MS_INVALID_PARA;
        }

        ms_compile_stream_state_destroy(stream->logical_vector,
                                          stream->combination_vector,
                                          stream->exhaustion_vector);
        stream->logical_vector = NULL;
        stream->logical_count = 0;
        stream->combination_vector = NULL;
        stream->combination_count = 0;
        stream->exhaustion_vector = NULL;
        stream->exhaustion_count = 0;
        ms_database_from_handler(&stream->database_view, stream->legacy_handler);
        stream->automaton = (ACAutomaton *)stream->database_view.ms_matcher;
        ret = ms_compile_stream_state_init(&stream->database_view,
                                             &stream->logical_vector,
                                             &stream->logical_count,
                                             &stream->combination_vector,
                                             &stream->combination_count,
                                             &stream->exhaustion_vector,
                                             &stream->exhaustion_count);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    } else if (stream->database == NULL) {
        return MS_INVALID_PARA;
    }

    stream->node_index = 0;
    stream->offset = 0;
    stream->generation = stream->automaton->generation;

    bytes = ms_compile_logical_byte_count(stream->logical_count);
    if (bytes != 0) {
        (void)memset(stream->logical_vector, 0, bytes);
    }
    bytes = ms_compile_logical_byte_count(stream->combination_count);
    if (bytes != 0) {
        (void)memset(stream->combination_vector, 0, bytes);
    }
    bytes = ms_compile_logical_byte_count(stream->exhaustion_count);
    if (bytes != 0) {
        (void)memset(stream->exhaustion_vector, 0, bytes);
    }

    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_close_handler_stream(ms_stream_t *stream)
{
    if (stream == NULL) {
        return MS_INVALID_PARA;
    }

    ms_compile_stream_state_destroy(stream->logical_vector,
                                      stream->combination_vector,
                                      stream->exhaustion_vector);
    ms_stream_free(stream);
    return MS_SUCCESS;
}

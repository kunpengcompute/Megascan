/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>
#include <string.h>

#include "expr/ms_expr_internal.h"
#include "ms.h"
#include "ms_ac.h"
#include "ms_acdb.h"
#include "ms_allocator.h"
#include "ms_rule_internal.h"
#include "ms_runtime.h"
#include "ms_stream_internal.h"

static void ms_stream_dirty_bytes_destroy(ms_stream_dirty_bytes_t *dirty) {
  if (dirty == NULL) {
    return;
  }

  ms_stream_free(dirty->marked);
  ms_stream_free(dirty->indexes);
  (void)memset(dirty, 0, sizeof(*dirty));
}

static void ms_stream_dirty_bytes_clear(ms_stream_dirty_bytes_t *dirty,
                                        uint8_t *vector) {
  uint32_t i;

  if (dirty == NULL || vector == NULL) {
    return;
  }
  for (i = 0; i < dirty->count; i++) {
    uint32_t index = dirty->indexes[i];

    if (index < dirty->byte_count) {
      vector[index] = 0;
      dirty->marked[index] = 0;
    }
  }
  dirty->count = 0;
}

static void ms_stream_dirty_state_destroy(ms_stream_t *stream) {
  if (stream == NULL) {
    return;
  }
  ms_stream_dirty_bytes_destroy(&stream->logical_dirty);
  ms_stream_dirty_bytes_destroy(&stream->combination_dirty);
  ms_stream_dirty_bytes_destroy(&stream->exhaustion_dirty);
}

static void ms_stream_dirty_state_clear(ms_stream_t *stream) {
  ms_stream_dirty_bytes_clear(&stream->logical_dirty, stream->logical_vector);
  ms_stream_dirty_bytes_clear(&stream->combination_dirty,
                              stream->combination_vector);
  ms_stream_dirty_bytes_clear(&stream->exhaustion_dirty,
                              stream->exhaustion_vector);
}

static int ms_open_stream_view(const ms_database_t *database,
                               ms_handler_t *legacy_handler,
                               const ms_database_t *db_view,
                               ms_stream_t **stream) {
  int ret;
  ms_stream_t *s = NULL;

  if (db_view == NULL || stream == NULL || *stream != NULL) {
    return MS_INVALID_PARA;
  }
  if (db_view->type != MS_MATCHER_TYPE_AC || db_view->ms_matcher == NULL ||
      !db_view->is_built) {
    return MS_INVALID_PARA;
  }

  s = (ms_stream_t *)ms_stream_calloc(1, sizeof(ms_stream_t));
  if (s == NULL) {
    return MS_MALLOC_FAIL;
  }

  s->database = database;
  s->legacy_handler = legacy_handler;
  s->database_view = *db_view;
  if (db_view->backend == MS_DATABASE_BACKEND_COMPACT_AC) {
    s->acdb = (const ms_acdb_t *)db_view->ms_matcher;
    s->generation = ms_acdb_generation(s->acdb);
  } else {
    s->automaton = (ACAutomaton *)db_view->ms_matcher;
    s->generation = s->automaton->generation;
  }
  ret = ms_compile_stream_state_init(
      &s->database_view, &s->logical_vector, &s->logical_count,
      &s->combination_vector, &s->combination_count, &s->exhaustion_vector,
      &s->exhaustion_count);
  if (ret != MS_SUCCESS) {
    ms_stream_free(s);
    return ret;
  }

  *stream = s;
  return MS_SUCCESS;
}

static const ms_database_t *
ms_stream_expected_database(const ms_stream_t *stream) {
  if (stream == NULL) {
    return NULL;
  }

  return (stream->database != NULL) ? stream->database : &stream->database_view;
}

static int ms_stream_call_with_temporary_scratch(
    ms_stream_t *stream, const uint8_t *data, uint32_t len, uint32_t *ids,
    uint32_t *id_count, uint32_t id_capacity, bool flush_only) {
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
    ret =
        ms_scan_stream(stream, data, len, scratch, ids, id_count, id_capacity);
  }
  free_ret = ms_free_scratch(scratch);
  if ((ret == MS_SUCCESS || ret == MS_FIND_FAIL) && free_ret != MS_SUCCESS) {
    return free_ret;
  }

  return ret;
}

__attribute__((visibility("default"))) int
ms_open_handler_stream(ms_handler_t *handler, ms_stream_t **stream) {
  ms_database_t db_view = {0};

  if (handler == NULL) {
    return MS_INVALID_PARA;
  }

  ms_database_from_handler(&db_view, handler);
  return ms_open_stream_view(NULL, handler, &db_view, stream);
}

__attribute__((visibility("default"))) int
ms_open_stream(const ms_database_t *db, ms_stream_t **stream) {
  if (db == NULL || db->type != MS_MATCHER_TYPE_AC || db->ms_matcher == NULL ||
      !db->is_built || db->ms_compile_ctx == NULL) {
    return MS_INVALID_PARA;
  }

  return ms_open_stream_view(db, NULL, db, stream);
}

int ms_stream_scan_direct(ms_stream_t *stream, const uint8_t *data,
                          uint32_t len) {
  int ret;

  if (stream == NULL || stream->automaton == NULL || data == NULL) {
    return MS_INVALID_PARA;
  }

  if (stream->generation != stream->automaton->generation) {
    return MS_STREAM_STALE;
  }

  ret = ac_automaton_match_state(stream->automaton, data, len,
                                 &stream->node_index);
  if (ret == MS_SUCCESS || ret == MS_FIND_FAIL) {
    stream->offset += len;
  }

  return ret;
}

__attribute__((visibility("default"))) int
ms_scan_rule_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len) {
  ms_compile_context_t *ctx;
  uint32_t *ids = NULL;
  uint32_t id_count = 0;
  uint32_t id_capacity;
  int ret;

  if (stream == NULL || (stream->automaton == NULL && stream->acdb == NULL) ||
      data == NULL) {
    return MS_INVALID_PARA;
  }

  if ((stream->automaton != NULL &&
       stream->generation != stream->automaton->generation) ||
      (stream->acdb != NULL &&
       stream->generation != ms_acdb_generation(stream->acdb))) {
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

__attribute__((visibility("default"))) int
ms_scan_expr_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len,
                    uint32_t *ids, uint32_t *id_count, uint32_t id_capacity) {
  if (stream == NULL || (stream->automaton == NULL && stream->acdb == NULL) ||
      data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
    return MS_INVALID_PARA;
  }

  if ((stream->automaton != NULL &&
       stream->generation != stream->automaton->generation) ||
      (stream->acdb != NULL &&
       stream->generation != ms_acdb_generation(stream->acdb))) {
    return MS_STREAM_STALE;
  }

  if (stream->database_view.ms_compile_ctx == NULL) {
    return MS_INVALID_PARA;
  }

  return ms_stream_call_with_temporary_scratch(stream, data, len, ids, id_count,
                                               id_capacity, false);
}

__attribute__((visibility("default"))) int
ms_scan_expr_stream_flush(ms_stream_t *stream, uint32_t *ids,
                          uint32_t *id_count, uint32_t id_capacity) {
  if (stream == NULL || (stream->automaton == NULL && stream->acdb == NULL) ||
      id_count == NULL || (id_capacity != 0 && ids == NULL)) {
    return MS_INVALID_PARA;
  }

  if ((stream->automaton != NULL &&
       stream->generation != stream->automaton->generation) ||
      (stream->acdb != NULL &&
       stream->generation != ms_acdb_generation(stream->acdb))) {
    return MS_STREAM_STALE;
  }

  return ms_stream_call_with_temporary_scratch(stream, NULL, 0, ids, id_count,
                                               id_capacity, true);
}

__attribute__((visibility("default"))) int
ms_reset_stream(ms_stream_t *stream) {
  uint8_t *logical_vector = NULL;
  uint8_t *combination_vector = NULL;
  uint8_t *exhaustion_vector = NULL;
  uint32_t logical_count = 0;
  uint32_t combination_count = 0;
  uint32_t exhaustion_count = 0;
  int ret;

  if (stream == NULL || (stream->automaton == NULL && stream->acdb == NULL)) {
    return MS_INVALID_PARA;
  }

  if (stream->legacy_handler != NULL) {
    if (stream->legacy_handler->type != MS_MATCHER_TYPE_AC ||
        stream->legacy_handler->ms_matcher == NULL ||
        !stream->legacy_handler->is_built) {
      return MS_INVALID_PARA;
    }

    ms_stream_dirty_state_destroy(stream);
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
    stream->acdb = NULL;
    ret = ms_compile_stream_state_init(
        &stream->database_view, &stream->logical_vector, &stream->logical_count,
        &stream->combination_vector, &stream->combination_count,
        &stream->exhaustion_vector, &stream->exhaustion_count);
    if (ret != MS_SUCCESS) {
      return ret;
    }
  } else {
    const ms_database_t *db = stream->database;
    uint64_t generation;

    if (db == NULL || db->type != MS_MATCHER_TYPE_AC ||
        db->ms_matcher == NULL || db->ms_compile_ctx == NULL || !db->is_built) {
      return MS_INVALID_PARA;
    }
    generation = db->backend == MS_DATABASE_BACKEND_COMPACT_AC
                     ? ms_acdb_generation((const ms_acdb_t *)db->ms_matcher)
                     : ((const ACAutomaton *)db->ms_matcher)->generation;
    if (stream->generation != generation) {
      ret = ms_compile_stream_state_init(
          db, &logical_vector, &logical_count, &combination_vector,
          &combination_count, &exhaustion_vector, &exhaustion_count);
      if (ret != MS_SUCCESS) {
        return ret;
      }
      ms_stream_dirty_state_destroy(stream);
      ms_compile_stream_state_destroy(stream->logical_vector,
                                      stream->combination_vector,
                                      stream->exhaustion_vector);
      stream->database_view = *db;
      stream->automaton = db->backend == MS_DATABASE_BACKEND_COMPACT_AC
                              ? NULL
                              : (ACAutomaton *)db->ms_matcher;
      stream->acdb = db->backend == MS_DATABASE_BACKEND_COMPACT_AC
                         ? (const ms_acdb_t *)db->ms_matcher
                         : NULL;
      stream->logical_vector = logical_vector;
      stream->logical_count = logical_count;
      stream->combination_vector = combination_vector;
      stream->combination_count = combination_count;
      stream->exhaustion_vector = exhaustion_vector;
      stream->exhaustion_count = exhaustion_count;
    }
  }

  stream->node_index = 0;
  stream->offset = 0;
  stream->generation = stream->acdb != NULL ? ms_acdb_generation(stream->acdb)
                                            : stream->automaton->generation;

  ms_stream_dirty_state_clear(stream);

  return MS_SUCCESS;
}

__attribute__((visibility("default"))) int
ms_close_handler_stream(ms_stream_t *stream) {
  if (stream == NULL) {
    return MS_INVALID_PARA;
  }

  ms_stream_dirty_state_destroy(stream);
  ms_compile_stream_state_destroy(stream->logical_vector,
                                  stream->combination_vector,
                                  stream->exhaustion_vector);
  ms_stream_free(stream);
  return MS_SUCCESS;
}

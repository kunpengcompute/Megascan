/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ms_acdb.h"
#include "ms_allocator.h"

#define MS_ACDB_ROOT 0u
#define MS_ACDB_NO_STATE UINT32_MAX
#define MS_ACDB_LINEAR_EDGE_LIMIT 8u

typedef struct {
  uint32_t fail;
  uint32_t output_link;
  uint32_t edge_begin;
  uint32_t edge_count;
  uint32_t output_begin;
  uint32_t output_count;
} ms_acdb_state_t;

typedef struct {
  uint32_t target;
  uint8_t ch;
  uint8_t reserved[3];
} ms_acdb_edge_t;

typedef struct {
  const uint8_t *key;
  uint32_t len;
  uint32_t alias;
} ms_acdb_rule_ref_t;

struct ms_acdb {
  ms_acdb_state_t *states;
  ms_acdb_edge_t *edges;
  uint32_t *outputs;
  uint32_t root_next[BYTE_MAX_SIZE];
  uint32_t state_count;
  uint32_t edge_count;
  uint32_t output_count;
  uint64_t generation;
};

static int ms_acdb_rule_compare(const void *lhs, const void *rhs) {
  const ms_acdb_rule_ref_t *a = (const ms_acdb_rule_ref_t *)lhs;
  const ms_acdb_rule_ref_t *b = (const ms_acdb_rule_ref_t *)rhs;
  uint32_t common = a->len < b->len ? a->len : b->len;
  int cmp = memcmp(a->key, b->key, common);

  if (cmp != 0) {
    return cmp;
  }
  if (a->len < b->len) {
    return -1;
  }
  if (a->len > b->len) {
    return 1;
  }
  if (a->alias < b->alias) {
    return -1;
  }
  return a->alias > b->alias;
}

static uint32_t ms_acdb_common_prefix(const ms_acdb_rule_ref_t *a,
                                      const ms_acdb_rule_ref_t *b) {
  uint32_t limit;
  uint32_t i = 0;

  if (a == NULL || b == NULL) {
    return 0;
  }
  limit = a->len < b->len ? a->len : b->len;
  while (i < limit && a->key[i] == b->key[i]) {
    i++;
  }
  return i;
}

static int ms_acdb_size_is_valid(size_t count, size_t element_size) {
  return element_size == 0 || count <= SIZE_MAX / element_size;
}

static uint32_t ms_acdb_find_edge(const ms_acdb_t *acdb, uint32_t state,
                                  uint8_t ch) {
  const ms_acdb_state_t *s = &acdb->states[state];
  uint32_t begin = s->edge_begin;
  uint32_t end = begin + s->edge_count;

  if (s->edge_count <= MS_ACDB_LINEAR_EDGE_LIMIT) {
    uint32_t i;

    for (i = begin; i < end; i++) {
      if (acdb->edges[i].ch == ch) {
        return acdb->edges[i].target;
      }
    }
    return MS_ACDB_NO_STATE;
  }

  while (begin < end) {
    uint32_t middle = begin + (end - begin) / 2u;
    uint8_t edge_ch = acdb->edges[middle].ch;

    if (edge_ch < ch) {
      begin = middle + 1u;
    } else if (edge_ch > ch) {
      end = middle;
    } else {
      return acdb->edges[middle].target;
    }
  }
  return MS_ACDB_NO_STATE;
}

static uint32_t ms_acdb_transition(const ms_acdb_t *acdb, uint32_t state,
                                   uint8_t ch) {
  uint32_t next;

  if (state == MS_ACDB_ROOT) {
    return acdb->root_next[ch];
  }

  next = ms_acdb_find_edge(acdb, state, ch);
  while (next == MS_ACDB_NO_STATE && state != MS_ACDB_ROOT) {
    state = acdb->states[state].fail;
    if (state == MS_ACDB_ROOT) {
      return acdb->root_next[ch];
    }
    next = ms_acdb_find_edge(acdb, state, ch);
  }
  return next == MS_ACDB_NO_STATE ? MS_ACDB_ROOT : next;
}

static int ms_acdb_build_failure(ms_acdb_t *acdb, uint32_t *queue) {
  uint32_t front = 0;
  uint32_t rear = 0;
  uint32_t i;

  if (queue == NULL) {
    return MS_INVALID_PARA;
  }

  for (i = 0; i < acdb->states[MS_ACDB_ROOT].edge_count; i++) {
    const ms_acdb_edge_t *edge =
        &acdb->edges[acdb->states[MS_ACDB_ROOT].edge_begin + i];

    acdb->root_next[edge->ch] = edge->target;
    acdb->states[edge->target].fail = MS_ACDB_ROOT;
    acdb->states[edge->target].output_link = MS_ACDB_ROOT;
    queue[rear++] = edge->target;
  }

  while (front < rear) {
    uint32_t parent = queue[front++];
    const ms_acdb_state_t *parent_state = &acdb->states[parent];

    for (i = 0; i < parent_state->edge_count; i++) {
      const ms_acdb_edge_t *edge = &acdb->edges[parent_state->edge_begin + i];
      uint32_t fail = acdb->states[parent].fail;
      uint32_t target = ms_acdb_transition(acdb, fail, edge->ch);

      acdb->states[edge->target].fail = target;
      acdb->states[edge->target].output_link =
          acdb->states[target].output_count != 0
              ? target
              : acdb->states[target].output_link;
      queue[rear++] = edge->target;
    }
  }

  return MS_SUCCESS;
}

static void ms_acdb_clear_compiled(ms_acdb_t *acdb) {
  if (acdb == NULL) {
    return;
  }

  ms_database_free(acdb->states);
  ms_database_free(acdb->edges);
  ms_database_free(acdb->outputs);
  acdb->states = NULL;
  acdb->edges = NULL;
  acdb->outputs = NULL;
  acdb->state_count = 0;
  acdb->edge_count = 0;
  acdb->output_count = 0;
  (void)memset(acdb->root_next, 0, sizeof(acdb->root_next));
}

int ms_acdb_create(ms_acdb_t **acdb) {
  ms_acdb_t *new_acdb;

  if (acdb == NULL || *acdb != NULL) {
    return MS_INVALID_PARA;
  }

  new_acdb = (ms_acdb_t *)ms_database_calloc(1, sizeof(ms_acdb_t));
  if (new_acdb == NULL) {
    return MS_MALLOC_FAIL;
  }
  *acdb = new_acdb;
  return MS_SUCCESS;
}

void ms_acdb_destroy(ms_acdb_t *acdb) {
  if (acdb == NULL) {
    return;
  }
  ms_acdb_clear_compiled(acdb);
  ms_database_free(acdb);
}

int ms_acdb_build(ms_acdb_t *acdb, const ms_rule_t *rules,
                  uint32_t rule_count) {
  ms_acdb_rule_ref_t *refs = NULL;
  uint32_t *parents = NULL;
  uint32_t *edge_fill = NULL;
  uint32_t *depth_stack = NULL;
  uint8_t *incoming = NULL;
  size_t max_states = 1;
  uint32_t max_len = 0;
  uint32_t state_count = 1;
  uint32_t output_count = 0;
  uint32_t i;
  int ret = MS_SUCCESS;

  if (acdb == NULL || (rule_count != 0 && rules == NULL) ||
      acdb->states != NULL) {
    return MS_INVALID_PARA;
  }

  if (rule_count == 0) {
    acdb->states =
        (ms_acdb_state_t *)ms_database_calloc(1, sizeof(*acdb->states));
    if (acdb->states == NULL) {
      return MS_MALLOC_FAIL;
    }
    acdb->state_count = 1;
    acdb->generation++;
    return MS_SUCCESS;
  }

  if (!ms_acdb_size_is_valid(rule_count, sizeof(*refs)) ||
      !ms_acdb_size_is_valid(rule_count, sizeof(uint32_t))) {
    return MS_MALLOC_FAIL;
  }
  refs = (ms_acdb_rule_ref_t *)ms_database_malloc((size_t)rule_count *
                                                  sizeof(*refs));
  if (refs == NULL) {
    return MS_MALLOC_FAIL;
  }

  /* Sorting lets us create the trie from adjacent LCPs without node maps. */
  for (i = 0; i < rule_count; i++) {
    if (rules[i].key == NULL || rules[i].key_len == 0) {
      ret = MS_INVALID_PARA;
      goto end;
    }
    if (rules[i].key_len > max_len) {
      max_len = rules[i].key_len;
    }
    refs[i].key = rules[i].key;
    refs[i].len = rules[i].key_len;
    refs[i].alias = rules[i].alias;
  }
  qsort(refs, rule_count, sizeof(*refs), ms_acdb_rule_compare);

  max_states = 1;
  for (i = 0; i < rule_count; i++) {
    uint32_t common =
        i == 0 ? 0 : ms_acdb_common_prefix(&refs[i - 1], &refs[i]);
    uint32_t new_states = refs[i].len - common;

    if (max_states > (size_t)UINT32_MAX - new_states) {
      ret = MS_MALLOC_FAIL;
      goto end;
    }
    max_states += new_states;
  }

  if (!ms_acdb_size_is_valid(max_states, sizeof(*acdb->states)) ||
      !ms_acdb_size_is_valid(max_states, sizeof(*parents)) ||
      !ms_acdb_size_is_valid(max_states, sizeof(*incoming)) ||
      !ms_acdb_size_is_valid((size_t)max_len + 1u, sizeof(*depth_stack))) {
    ret = MS_MALLOC_FAIL;
    goto end;
  }

  acdb->states =
      (ms_acdb_state_t *)ms_database_calloc(max_states, sizeof(*acdb->states));
  parents = (uint32_t *)ms_database_malloc(max_states * sizeof(*parents));
  incoming = (uint8_t *)ms_database_malloc(max_states * sizeof(*incoming));
  depth_stack = (uint32_t *)ms_database_malloc(((size_t)max_len + 1u) *
                                               sizeof(*depth_stack));
  acdb->outputs = (uint32_t *)ms_database_malloc((size_t)rule_count *
                                                 sizeof(*acdb->outputs));
  if (acdb->states == NULL || parents == NULL || incoming == NULL ||
      depth_stack == NULL || acdb->outputs == NULL) {
    ret = MS_MALLOC_FAIL;
    goto end;
  }

  parents[MS_ACDB_ROOT] = MS_ACDB_ROOT;
  incoming[MS_ACDB_ROOT] = 0;
  depth_stack[0] = MS_ACDB_ROOT;
  for (i = 0; i < rule_count; i++) {
    uint32_t common =
        i == 0 ? 0 : ms_acdb_common_prefix(&refs[i - 1], &refs[i]);
    uint32_t state = depth_stack[common];
    uint32_t depth;

    for (depth = common; depth < refs[i].len; depth++) {
      uint32_t child = state_count++;

      parents[child] = state;
      incoming[child] = refs[i].key[depth];
      acdb->states[state].edge_count++;
      depth_stack[depth + 1u] = child;
      state = child;
    }

    if (acdb->states[state].output_count == 0) {
      acdb->states[state].output_begin = output_count;
    }
    acdb->outputs[output_count++] = refs[i].alias;
    acdb->states[state].output_count++;
  }

  acdb->state_count = state_count;
  acdb->edge_count = state_count - 1u;
  acdb->output_count = output_count;
  if (!ms_acdb_size_is_valid(acdb->edge_count, sizeof(*acdb->edges)) ||
      !ms_acdb_size_is_valid(state_count, sizeof(*edge_fill))) {
    ret = MS_MALLOC_FAIL;
    goto end;
  }
  acdb->edges = (ms_acdb_edge_t *)ms_database_calloc(acdb->edge_count,
                                                     sizeof(*acdb->edges));
  edge_fill = (uint32_t *)ms_database_calloc(state_count, sizeof(*edge_fill));
  if (acdb->edges == NULL || edge_fill == NULL) {
    ret = MS_MALLOC_FAIL;
    goto end;
  }

  output_count = 0;
  for (i = 0; i < state_count; i++) {
    uint32_t count = acdb->states[i].edge_count;

    acdb->states[i].edge_begin = output_count;
    output_count += count;
  }
  for (i = 1; i < state_count; i++) {
    uint32_t parent = parents[i];
    uint32_t edge_index = acdb->states[parent].edge_begin + edge_fill[parent]++;

    acdb->edges[edge_index].ch = incoming[i];
    acdb->edges[edge_index].target = i;
  }

  ms_database_free(edge_fill);
  edge_fill = NULL;
  ms_database_free(depth_stack);
  depth_stack = NULL;
  ms_database_free(incoming);
  incoming = NULL;
  ms_database_free(refs);
  refs = NULL;

  /* The parent array is no longer needed, so reuse it as the BFS queue. */
  ret = ms_acdb_build_failure(acdb, parents);
  if (ret != MS_SUCCESS) {
    goto end;
  }
  acdb->generation++;

end:
  ms_database_free(edge_fill);
  ms_database_free(depth_stack);
  ms_database_free(incoming);
  ms_database_free(parents);
  ms_database_free(refs);
  if (ret != MS_SUCCESS) {
    ms_acdb_clear_compiled(acdb);
  }
  return ret;
}

static int ms_acdb_export_rules(const ms_acdb_t *acdb, ms_rule_t **rules_out,
                                uint32_t *rule_count_out, uint8_t **keys_out) {
  ms_rule_t *rules = NULL;
  uint8_t *keys = NULL;
  uint32_t *parents = NULL;
  uint32_t *depths = NULL;
  uint8_t *incoming = NULL;
  size_t key_bytes = 0;
  size_t key_offset = 0;
  uint32_t rule_index = 0;
  uint32_t i;
  int ret = MS_SUCCESS;

  if (acdb == NULL || acdb->states == NULL || rules_out == NULL ||
      rule_count_out == NULL || keys_out == NULL || *rules_out != NULL ||
      *keys_out != NULL) {
    return MS_INVALID_PARA;
  }
  *rule_count_out = 0;
  if (acdb->output_count == 0) {
    return MS_SUCCESS;
  }
  if (!ms_acdb_size_is_valid(acdb->state_count, sizeof(uint32_t)) ||
      !ms_acdb_size_is_valid(acdb->state_count, sizeof(uint8_t)) ||
      !ms_acdb_size_is_valid(acdb->output_count, sizeof(ms_rule_t))) {
    return MS_MALLOC_FAIL;
  }
  /* Reconstruct keys only during deletion; normal databases keep no rule copy.
   */
  parents = (uint32_t *)ms_database_calloc(acdb->state_count, sizeof(uint32_t));
  depths = (uint32_t *)ms_database_calloc(acdb->state_count, sizeof(uint32_t));
  incoming = (uint8_t *)ms_database_calloc(acdb->state_count, sizeof(uint8_t));
  if (parents == NULL || depths == NULL || incoming == NULL) {
    ret = MS_MALLOC_FAIL;
    goto end;
  }
  for (i = 0; i < acdb->state_count; i++) {
    const ms_acdb_state_t *state = &acdb->states[i];
    uint32_t j;

    if (state->output_count != 0) {
      if (depths[i] != 0 &&
          state->output_count > (SIZE_MAX - key_bytes) / depths[i]) {
        ret = MS_MALLOC_FAIL;
        goto end;
      }
      key_bytes += (size_t)depths[i] * state->output_count;
    }
    for (j = 0; j < state->edge_count; j++) {
      const ms_acdb_edge_t *edge = &acdb->edges[state->edge_begin + j];

      parents[edge->target] = i;
      incoming[edge->target] = edge->ch;
      depths[edge->target] = depths[i] + 1u;
    }
  }
  rules =
      (ms_rule_t *)ms_database_calloc(acdb->output_count, sizeof(ms_rule_t));
  keys = (uint8_t *)ms_database_malloc(key_bytes);
  if (rules == NULL || keys == NULL) {
    ret = MS_MALLOC_FAIL;
    goto end;
  }
  for (i = 0; i < acdb->state_count; i++) {
    const ms_acdb_state_t *state = &acdb->states[i];
    uint32_t j;

    for (j = 0; j < state->output_count; j++) {
      uint32_t cursor = i;
      uint32_t depth = depths[i];
      uint32_t k;

      rules[rule_index].key = keys + key_offset;
      rules[rule_index].key_len = depth;
      rules[rule_index].alias = acdb->outputs[state->output_begin + j];
      for (k = depth; k > 0; k--) {
        rules[rule_index].key[k - 1u] = incoming[cursor];
        cursor = parents[cursor];
      }
      key_offset += depth;
      rule_index++;
    }
  }
  *rules_out = rules;
  *keys_out = keys;
  *rule_count_out = rule_index;
  rules = NULL;
  keys = NULL;

end:
  ms_database_free(incoming);
  ms_database_free(depths);
  ms_database_free(parents);
  ms_database_free(keys);
  ms_database_free(rules);
  return ret;
}

int ms_acdb_prepare_remove(const ms_acdb_t *acdb, const ms_rule_t *rules,
                           uint32_t remove_count, ms_acdb_t **candidate) {
  ms_rule_t *exported = NULL;
  ms_rule_t *remaining = NULL;
  uint8_t *keys = NULL;
  uint8_t *removed = NULL;
  ms_acdb_t *new_acdb = NULL;
  uint32_t rule_count = 0;
  uint32_t i;
  uint32_t out = 0;
  int ret;

  if (acdb == NULL || rules == NULL || remove_count == 0 || candidate == NULL ||
      *candidate != NULL) {
    return MS_INVALID_PARA;
  }
  ret = ms_acdb_export_rules(acdb, &exported, &rule_count, &keys);
  if (ret != MS_SUCCESS) {
    return ret;
  }
  if (rule_count < remove_count) {
    ret = MS_FIND_FAIL;
    goto end;
  }
  removed = (uint8_t *)ms_database_calloc(rule_count, sizeof(uint8_t));
  if (removed == NULL) {
    ret = MS_MALLOC_FAIL;
    goto end;
  }
  for (i = 0; i < remove_count; i++) {
    uint32_t j;
    bool found = false;

    if (rules[i].key == NULL || rules[i].key_len == 0) {
      ret = MS_INVALID_PARA;
      goto end;
    }
    for (j = 0; j < rule_count; j++) {
      if (removed[j] == 0 && exported[j].alias == rules[i].alias &&
          exported[j].key_len == rules[i].key_len &&
          memcmp(exported[j].key, rules[i].key, rules[i].key_len) == 0) {
        removed[j] = 1;
        found = true;
        break;
      }
    }
    if (!found) {
      ret = MS_FIND_FAIL;
      goto end;
    }
  }

  if (rule_count > remove_count) {
    remaining = (ms_rule_t *)ms_database_calloc(rule_count - remove_count,
                                                sizeof(ms_rule_t));
    if (remaining == NULL) {
      ret = MS_MALLOC_FAIL;
      goto end;
    }
    for (i = 0; i < rule_count; i++) {
      if (removed[i] == 0) {
        remaining[out++] = exported[i];
      }
    }
  }

  ret = ms_acdb_create(&new_acdb);
  if (ret == MS_SUCCESS) {
    ret = ms_acdb_build(new_acdb, remaining, rule_count - remove_count);
  }

end:
  ms_database_free(remaining);
  ms_database_free(removed);
  ms_database_free(keys);
  ms_database_free(exported);
  if (ret != MS_SUCCESS) {
    ms_acdb_destroy(new_acdb);
    return ret;
  }
  *candidate = new_acdb;
  return MS_SUCCESS;
}

void ms_acdb_commit_rebuild(ms_acdb_t *acdb, ms_acdb_t *candidate) {
  uint64_t generation;

  if (acdb == NULL || candidate == NULL) {
    return;
  }
  /* Keep the wrapper address stable so existing streams can detect staleness.
   */
  generation = acdb->generation + 1u;
  ms_acdb_clear_compiled(acdb);
  *acdb = *candidate;
  acdb->generation = generation;
  (void)memset(candidate, 0, sizeof(*candidate));
  ms_database_free(candidate);
}

void ms_acdb_bump_generation(ms_acdb_t *acdb) {
  if (acdb != NULL) {
    acdb->generation++;
  }
}

static int ms_acdb_emit_state(const ms_acdb_t *acdb, uint32_t state,
                              uint64_t end_offset, ms_ac_match_cb cb,
                              void *ctx) {
  uint32_t output_state = state;

  while (output_state != MS_ACDB_ROOT) {
    const ms_acdb_state_t *s = &acdb->states[output_state];
    uint32_t i;

    for (i = 0; i < s->output_count; i++) {
      int ret = cb(acdb->outputs[s->output_begin + i], end_offset, ctx);

      if (ret != MS_SUCCESS) {
        return ret;
      }
    }
    output_state = s->output_link;
  }
  return MS_SUCCESS;
}

int ms_acdb_match_state_cb(const ms_acdb_t *acdb, const uint8_t *data,
                           uint32_t len, uint32_t *state, ms_ac_match_cb cb,
                           void *ctx) {
  uint32_t current;
  int match_ret = MS_FIND_FAIL;
  uint32_t i;

  if (acdb == NULL || acdb->states == NULL || data == NULL || state == NULL ||
      cb == NULL || *state >= acdb->state_count) {
    return MS_INVALID_PARA;
  }

  current = *state;
  for (i = 0; i < len; i++) {
    int ret;

    current = ms_acdb_transition(acdb, current, data[i]);
    if (acdb->states[current].output_count == 0 &&
        acdb->states[current].output_link == MS_ACDB_ROOT) {
      continue;
    }

    match_ret = MS_SUCCESS;
    ret = ms_acdb_emit_state(acdb, current, (uint64_t)i + 1u, cb, ctx);
    if (ret != MS_SUCCESS) {
      *state = current;
      return ret;
    }
  }

  *state = current;
  return match_ret;
}

static int ms_acdb_match_cb(uint32_t alias, uint64_t end_offset, void *ctx) {
  (void)alias;
  (void)end_offset;
  (void)ctx;
  return MS_SUCCESS;
}

int ms_acdb_match(const ms_acdb_t *acdb, const uint8_t *data, uint32_t len) {
  uint32_t state = MS_ACDB_ROOT;

  return ms_acdb_match_state_cb(acdb, data, len, &state, ms_acdb_match_cb,
                                NULL);
}

uint64_t ms_acdb_generation(const ms_acdb_t *acdb) {
  return acdb == NULL ? 0 : acdb->generation;
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "ms_allocator.h"
#include "ms.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MALLOC malloc
#define DEFAULT_CALLOC calloc
#define DEFAULT_FREE free

#define MS_ALLOC_ALIGN 64

typedef union {
  struct {
    ms_free_t free_func;
  } metadata;
  max_align_t alignment;
} ms_alloc_header_t;

static ms_alloc_t g_ms_db_alloc = DEFAULT_MALLOC;
static ms_calloc_t g_ms_db_calloc = DEFAULT_CALLOC;
static ms_free_t g_ms_db_free = DEFAULT_FREE;

static ms_alloc_t g_ms_scratch_alloc = DEFAULT_MALLOC;
static ms_calloc_t g_ms_scratch_calloc = DEFAULT_CALLOC;
static ms_free_t g_ms_scratch_free = DEFAULT_FREE;

static ms_alloc_t g_ms_stream_alloc = DEFAULT_MALLOC;
static ms_calloc_t g_ms_stream_calloc = DEFAULT_CALLOC;
static ms_free_t g_ms_stream_free = DEFAULT_FREE;

static ms_alloc_t g_ms_misc_alloc = DEFAULT_MALLOC;
static ms_calloc_t g_ms_misc_calloc = DEFAULT_CALLOC;
static ms_free_t g_ms_misc_free = DEFAULT_FREE;

static ms_alloc_t normalise_alloc(ms_alloc_t a) {
  return a ? a : DEFAULT_MALLOC;
}

static ms_calloc_t normalise_calloc(ms_calloc_t c) {
  return c ? c : DEFAULT_CALLOC;
}

static ms_free_t normalise_free(ms_free_t f) { return f ? f : DEFAULT_FREE; }

static void *tracked_alloc(ms_alloc_t alloc_func, ms_free_t free_func,
                           size_t size) {
  ms_alloc_header_t *header;

  if (size > SIZE_MAX - sizeof(*header)) {
    return NULL;
  }

  header = (ms_alloc_header_t *)alloc_func(sizeof(*header) + size);
  if (header == NULL) {
    return NULL;
  }

  header->metadata.free_func = free_func;
  return (void *)(header + 1);
}

static void *tracked_calloc(ms_calloc_t calloc_func, ms_free_t free_func,
                            size_t nmemb, size_t size) {
  ms_alloc_header_t *header;
  size_t user_size;

  if (nmemb != 0 && size > (SIZE_MAX - sizeof(*header)) / nmemb) {
    return NULL;
  }
  user_size = nmemb * size;

  header = (ms_alloc_header_t *)calloc_func(1, sizeof(*header) + user_size);
  if (header == NULL) {
    return NULL;
  }

  header->metadata.free_func = free_func;
  return (void *)(header + 1);
}

static void tracked_free(void *ptr) {
  ms_alloc_header_t *header;
  ms_free_t free_func;

  if (ptr == NULL) {
    return;
  }

  header = ((ms_alloc_header_t *)ptr) - 1;
  free_func = header->metadata.free_func;
  free_func(header);
}

int ms_check_alloc(const void *mem) {
  if (!mem) {
    return MS_MALLOC_FAIL;
  }
  if (!ISALIGNED_N(mem, MS_ALLOC_ALIGN)) {
    return MS_INVALID_PARA;
  }
  return MS_SUCCESS;
}

__attribute__((visibility("default"))) int
ms_set_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                 ms_free_t free_func) {
  (void)ms_set_database_allocator(alloc_func, calloc_func, free_func);
  (void)ms_set_scratch_allocator(alloc_func, calloc_func, free_func);
  (void)ms_set_stream_allocator(alloc_func, calloc_func, free_func);
  (void)ms_set_misc_allocator(alloc_func, calloc_func, free_func);
  return MS_SUCCESS;
}

__attribute__((visibility("default"))) int
ms_set_database_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                          ms_free_t free_func) {
  g_ms_db_alloc = normalise_alloc(alloc_func);
  g_ms_db_calloc = normalise_calloc(calloc_func);
  g_ms_db_free = normalise_free(free_func);
  return MS_SUCCESS;
}

__attribute__((visibility("default"))) int
ms_set_scratch_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                         ms_free_t free_func) {
  g_ms_scratch_alloc = normalise_alloc(alloc_func);
  g_ms_scratch_calloc = normalise_calloc(calloc_func);
  g_ms_scratch_free = normalise_free(free_func);
  return MS_SUCCESS;
}

__attribute__((visibility("default"))) int
ms_set_stream_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                        ms_free_t free_func) {
  g_ms_stream_alloc = normalise_alloc(alloc_func);
  g_ms_stream_calloc = normalise_calloc(calloc_func);
  g_ms_stream_free = normalise_free(free_func);
  return MS_SUCCESS;
}

__attribute__((visibility("default"))) int
ms_set_misc_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                      ms_free_t free_func) {
  g_ms_misc_alloc = normalise_alloc(alloc_func);
  g_ms_misc_calloc = normalise_calloc(calloc_func);
  g_ms_misc_free = normalise_free(free_func);
  return MS_SUCCESS;
}

void *ms_malloc(size_t size) { return ms_database_malloc(size); }

void *ms_calloc(size_t nmemb, size_t size) {
  return ms_database_calloc(nmemb, size);
}

void ms_free(void *ptr) { ms_database_free(ptr); }

void *ms_database_malloc(size_t size) {
  return tracked_alloc(g_ms_db_alloc, g_ms_db_free, size);
}

void *ms_database_calloc(size_t nmemb, size_t size) {
  return tracked_calloc(g_ms_db_calloc, g_ms_db_free, nmemb, size);
}

void ms_database_free(void *ptr) { tracked_free(ptr); }

void *ms_scratch_malloc(size_t size) {
  return tracked_alloc(g_ms_scratch_alloc, g_ms_scratch_free, size);
}

void *ms_scratch_calloc(size_t nmemb, size_t size) {
  return tracked_calloc(g_ms_scratch_calloc, g_ms_scratch_free, nmemb, size);
}

void ms_scratch_free(void *ptr) { tracked_free(ptr); }

void *ms_stream_malloc(size_t size) {
  return tracked_alloc(g_ms_stream_alloc, g_ms_stream_free, size);
}

void *ms_stream_calloc(size_t nmemb, size_t size) {
  return tracked_calloc(g_ms_stream_calloc, g_ms_stream_free, nmemb, size);
}

void ms_stream_free(void *ptr) { tracked_free(ptr); }

void *ms_misc_malloc(size_t size) {
  return tracked_alloc(g_ms_misc_alloc, g_ms_misc_free, size);
}

void *ms_misc_calloc(size_t nmemb, size_t size) {
  return tracked_calloc(g_ms_misc_calloc, g_ms_misc_free, nmemb, size);
}

void ms_misc_free(void *ptr) { tracked_free(ptr); }

void *ms_database_aligned_alloc(size_t size, size_t align) {
  uintptr_t aligned;
  uintptr_t raw_addr;
  void *raw;

  if (align == 0 || (align & (align - 1)) != 0) {
    return NULL;
  }

  if (size > SIZE_MAX - (align - 1) - sizeof(void *)) {
    return NULL;
  }

  raw = ms_database_malloc(size + align - 1 + sizeof(void *));
  if (raw == NULL) {
    return NULL;
  }

  raw_addr = (uintptr_t)raw + sizeof(void *);
  aligned = (raw_addr + align - 1) & ~(uintptr_t)(align - 1);
  ((void **)aligned)[-1] = raw;
  return (void *)aligned;
}

void ms_database_aligned_free(void *ptr) {
  if (ptr != NULL) {
    ms_database_free(((void **)ptr)[-1]);
  }
}

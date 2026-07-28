/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ms_allocator.h"
#include "ms.h"

#define DEFAULT_MALLOC malloc
#define DEFAULT_CALLOC calloc
#define DEFAULT_FREE free

#define MS_ALLOC_ALIGN 64

ms_alloc_t g_ms_db_alloc = DEFAULT_MALLOC;
ms_calloc_t g_ms_db_calloc = DEFAULT_CALLOC;
ms_free_t g_ms_db_free = DEFAULT_FREE;

ms_alloc_t g_ms_scratch_alloc = DEFAULT_MALLOC;
ms_calloc_t g_ms_scratch_calloc = DEFAULT_CALLOC;
ms_free_t g_ms_scratch_free = DEFAULT_FREE;

ms_alloc_t g_ms_stream_alloc = DEFAULT_MALLOC;
ms_calloc_t g_ms_stream_calloc = DEFAULT_CALLOC;
ms_free_t g_ms_stream_free = DEFAULT_FREE;

ms_alloc_t g_ms_misc_alloc = DEFAULT_MALLOC;
ms_calloc_t g_ms_misc_calloc = DEFAULT_CALLOC;
ms_free_t g_ms_misc_free = DEFAULT_FREE;

static ms_alloc_t normalise_alloc(ms_alloc_t a)
{
    return a ? a : DEFAULT_MALLOC;
}

static ms_calloc_t normalise_calloc(ms_calloc_t c)
{
    return c ? c : DEFAULT_CALLOC;
}

static ms_free_t normalise_free(ms_free_t f)
{
    return f ? f : DEFAULT_FREE;
}

int ms_check_alloc(const void *mem)
{
    if (!mem) {
        return MS_MALLOC_FAIL;
    }
    if (!ISALIGNED_N(mem, MS_ALLOC_ALIGN)) {
        return MS_INVALID_PARA;
    }
    return MS_SUCCESS;
}

ms_alloc_t ms_get_db_alloc(void)
{
    return g_ms_db_alloc;
}

ms_calloc_t ms_get_db_calloc(void)
{
    return g_ms_db_calloc;
}

ms_free_t ms_get_db_free(void)
{
    return g_ms_db_free;
}

__attribute__((visibility("default")))
int ms_set_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func, ms_free_t free_func)
{
    (void)ms_set_database_allocator(alloc_func, calloc_func, free_func);
    (void)ms_set_scratch_allocator(alloc_func, calloc_func, free_func);
    (void)ms_set_stream_allocator(alloc_func, calloc_func, free_func);
    (void)ms_set_misc_allocator(alloc_func, calloc_func, free_func);
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_set_database_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                                           ms_free_t free_func)
{
    g_ms_db_alloc = normalise_alloc(alloc_func);
    g_ms_db_calloc = normalise_calloc(calloc_func);
    g_ms_db_free = normalise_free(free_func);
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_set_scratch_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                                          ms_free_t free_func)
{
    g_ms_scratch_alloc = normalise_alloc(alloc_func);
    g_ms_scratch_calloc = normalise_calloc(calloc_func);
    g_ms_scratch_free = normalise_free(free_func);
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_set_stream_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                                         ms_free_t free_func)
{
    g_ms_stream_alloc = normalise_alloc(alloc_func);
    g_ms_stream_calloc = normalise_calloc(calloc_func);
    g_ms_stream_free = normalise_free(free_func);
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_set_misc_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                                       ms_free_t free_func)
{
    g_ms_misc_alloc = normalise_alloc(alloc_func);
    g_ms_misc_calloc = normalise_calloc(calloc_func);
    g_ms_misc_free = normalise_free(free_func);
    return MS_SUCCESS;
}

void *ms_malloc(size_t size)
{
    return ms_database_malloc(size);
}

void *ms_calloc(size_t nmemb, size_t size)
{
    return ms_database_calloc(nmemb, size);
}

void ms_free(void *ptr)
{
    ms_database_free(ptr);
}

void *ms_database_malloc(size_t size)
{
    return g_ms_db_alloc(size);
}

void *ms_database_calloc(size_t nmemb, size_t size)
{
    return g_ms_db_calloc(nmemb, size);
}

void ms_database_free(void *ptr)
{
    if (ptr != NULL) {
        g_ms_db_free(ptr);
    }
}

void *ms_scratch_malloc(size_t size)
{
    return g_ms_scratch_alloc(size);
}

void *ms_scratch_calloc(size_t nmemb, size_t size)
{
    return g_ms_scratch_calloc(nmemb, size);
}

void ms_scratch_free(void *ptr)
{
    if (ptr != NULL) {
        g_ms_scratch_free(ptr);
    }
}

void *ms_stream_malloc(size_t size)
{
    return g_ms_stream_alloc(size);
}

void *ms_stream_calloc(size_t nmemb, size_t size)
{
    return g_ms_stream_calloc(nmemb, size);
}

void ms_stream_free(void *ptr)
{
    if (ptr != NULL) {
        g_ms_stream_free(ptr);
    }
}

void *ms_misc_malloc(size_t size)
{
    return g_ms_misc_alloc(size);
}

void *ms_misc_calloc(size_t nmemb, size_t size)
{
    return g_ms_misc_calloc(nmemb, size);
}

void ms_misc_free(void *ptr)
{
    if (ptr != NULL) {
        g_ms_misc_free(ptr);
    }
}

void *ms_database_aligned_alloc(size_t size, size_t align)
{
    uintptr_t aligned;
    uintptr_t raw_addr;
    void *raw;

    if (align == 0 || (align & (align - 1)) != 0) {
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

void ms_database_aligned_free(void *ptr)
{
    if (ptr != NULL) {
        ms_database_free(((void **)ptr)[-1]);
    }
}

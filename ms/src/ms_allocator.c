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
int MS_CDECL ms_set_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func, ms_free_t free_func)
{
    g_ms_db_alloc = normalise_alloc(alloc_func);
    g_ms_db_calloc = normalise_calloc(calloc_func);
    g_ms_db_free = normalise_free(free_func);
    return MS_SUCCESS;
}

void *ms_malloc(size_t size)
{
    void *ptr = g_ms_db_alloc(size);
    if (ptr == NULL) {
        return NULL;
    }
    return ptr;
}

void *ms_calloc(size_t nmemb, size_t size)
{
    void *ptr = g_ms_db_calloc(nmemb, size);
    if (ptr == NULL) {
        return NULL;
    }
    return ptr;
}

void ms_free(void *ptr)
{
    if (ptr != NULL) {
        g_ms_db_free(ptr);
    }
}

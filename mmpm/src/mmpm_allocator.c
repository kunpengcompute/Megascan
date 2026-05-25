/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mmpm_allocator.h"
#include "mmpm.h"

#define DEFAULT_MALLOC malloc
#define DEFAULT_CALLOC calloc
#define DEFAULT_FREE free

#define MMPM_ALLOC_ALIGN 64

mmpm_alloc_t g_mmpm_db_alloc = DEFAULT_MALLOC;
mmpm_calloc_t g_mmpm_db_calloc = DEFAULT_CALLOC;
mmpm_free_t g_mmpm_db_free = DEFAULT_FREE;

static mmpm_alloc_t normalise_alloc(mmpm_alloc_t a)
{
    return a ? a : DEFAULT_MALLOC;
}

static mmpm_calloc_t normalise_calloc(mmpm_calloc_t c)
{
    return c ? c : DEFAULT_CALLOC;
}

static mmpm_free_t normalise_free(mmpm_free_t f)
{
    return f ? f : DEFAULT_FREE;
}

int mmpm_check_alloc(const void *mem)
{
    if (!mem) {
        return MMPM_MALLOC_FAIL;
    }
    if (!ISALIGNED_N(mem, MMPM_ALLOC_ALIGN)) {
        return MMPM_INVALID_PARA;
    }
    return MMPM_SUCCESS;
}

mmpm_alloc_t mmpm_get_db_alloc(void)
{
    return g_mmpm_db_alloc;
}

mmpm_calloc_t mmpm_get_db_calloc(void)
{
    return g_mmpm_db_calloc;
}

mmpm_free_t mmpm_get_db_free(void)
{
    return g_mmpm_db_free;
}

int MMPM_CDECL mmpm_set_allocator(mmpm_alloc_t alloc_func, mmpm_calloc_t calloc_func, mmpm_free_t free_func)
{
    g_mmpm_db_alloc = normalise_alloc(alloc_func);
    g_mmpm_db_calloc = normalise_calloc(calloc_func);
    g_mmpm_db_free = normalise_free(free_func);
    return MMPM_SUCCESS;
}

void *mmpm_malloc(size_t size)
{
    void *ptr = g_mmpm_db_alloc(size);
    if (ptr == NULL) {
        return NULL;
    }
    return ptr;
}

void *mmpm_calloc(size_t nmemb, size_t size)
{
    void *ptr = g_mmpm_db_calloc(nmemb, size);
    if (ptr == NULL) {
        return NULL;
    }
    return ptr;
}

void mmpm_free(void *ptr)
{
    if (ptr != NULL) {
        g_mmpm_db_free(ptr);
    }
}
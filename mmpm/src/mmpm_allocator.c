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

mmpm_alloc_t g_mmpm_scratch_alloc = DEFAULT_MALLOC;
mmpm_calloc_t g_mmpm_scratch_calloc = DEFAULT_CALLOC;
mmpm_free_t g_mmpm_scratch_free = DEFAULT_FREE;

mmpm_alloc_t g_mmpm_stream_alloc = DEFAULT_MALLOC;
mmpm_calloc_t g_mmpm_stream_calloc = DEFAULT_CALLOC;
mmpm_free_t g_mmpm_stream_free = DEFAULT_FREE;

mmpm_alloc_t g_mmpm_misc_alloc = DEFAULT_MALLOC;
mmpm_calloc_t g_mmpm_misc_calloc = DEFAULT_CALLOC;
mmpm_free_t g_mmpm_misc_free = DEFAULT_FREE;

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

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_set_allocator(mmpm_alloc_t alloc_func, mmpm_calloc_t calloc_func, mmpm_free_t free_func)
{
    (void)mmpm_set_database_allocator(alloc_func, calloc_func, free_func);
    (void)mmpm_set_scratch_allocator(alloc_func, calloc_func, free_func);
    (void)mmpm_set_stream_allocator(alloc_func, calloc_func, free_func);
    (void)mmpm_set_misc_allocator(alloc_func, calloc_func, free_func);
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_set_database_allocator(mmpm_alloc_t alloc_func, mmpm_calloc_t calloc_func,
                                           mmpm_free_t free_func)
{
    g_mmpm_db_alloc = normalise_alloc(alloc_func);
    g_mmpm_db_calloc = normalise_calloc(calloc_func);
    g_mmpm_db_free = normalise_free(free_func);
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_set_scratch_allocator(mmpm_alloc_t alloc_func, mmpm_calloc_t calloc_func,
                                          mmpm_free_t free_func)
{
    g_mmpm_scratch_alloc = normalise_alloc(alloc_func);
    g_mmpm_scratch_calloc = normalise_calloc(calloc_func);
    g_mmpm_scratch_free = normalise_free(free_func);
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_set_stream_allocator(mmpm_alloc_t alloc_func, mmpm_calloc_t calloc_func,
                                         mmpm_free_t free_func)
{
    g_mmpm_stream_alloc = normalise_alloc(alloc_func);
    g_mmpm_stream_calloc = normalise_calloc(calloc_func);
    g_mmpm_stream_free = normalise_free(free_func);
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_set_misc_allocator(mmpm_alloc_t alloc_func, mmpm_calloc_t calloc_func,
                                       mmpm_free_t free_func)
{
    g_mmpm_misc_alloc = normalise_alloc(alloc_func);
    g_mmpm_misc_calloc = normalise_calloc(calloc_func);
    g_mmpm_misc_free = normalise_free(free_func);
    return MMPM_SUCCESS;
}

void *mmpm_malloc(size_t size)
{
    return mmpm_database_malloc(size);
}

void *mmpm_calloc(size_t nmemb, size_t size)
{
    return mmpm_database_calloc(nmemb, size);
}

void mmpm_free(void *ptr)
{
    mmpm_database_free(ptr);
}

void *mmpm_database_malloc(size_t size)
{
    return g_mmpm_db_alloc(size);
}

void *mmpm_database_calloc(size_t nmemb, size_t size)
{
    return g_mmpm_db_calloc(nmemb, size);
}

void mmpm_database_free(void *ptr)
{
    if (ptr != NULL) {
        g_mmpm_db_free(ptr);
    }
}

void *mmpm_scratch_malloc(size_t size)
{
    return g_mmpm_scratch_alloc(size);
}

void *mmpm_scratch_calloc(size_t nmemb, size_t size)
{
    return g_mmpm_scratch_calloc(nmemb, size);
}

void mmpm_scratch_free(void *ptr)
{
    if (ptr != NULL) {
        g_mmpm_scratch_free(ptr);
    }
}

void *mmpm_stream_malloc(size_t size)
{
    return g_mmpm_stream_alloc(size);
}

void *mmpm_stream_calloc(size_t nmemb, size_t size)
{
    return g_mmpm_stream_calloc(nmemb, size);
}

void mmpm_stream_free(void *ptr)
{
    if (ptr != NULL) {
        g_mmpm_stream_free(ptr);
    }
}

void *mmpm_misc_malloc(size_t size)
{
    return g_mmpm_misc_alloc(size);
}

void *mmpm_misc_calloc(size_t nmemb, size_t size)
{
    return g_mmpm_misc_calloc(nmemb, size);
}

void mmpm_misc_free(void *ptr)
{
    if (ptr != NULL) {
        g_mmpm_misc_free(ptr);
    }
}

void *mmpm_database_aligned_alloc(size_t size, size_t align)
{
    uintptr_t aligned;
    uintptr_t raw_addr;
    void *raw;

    if (align == 0 || (align & (align - 1)) != 0) {
        return NULL;
    }

    raw = mmpm_database_malloc(size + align - 1 + sizeof(void *));
    if (raw == NULL) {
        return NULL;
    }

    raw_addr = (uintptr_t)raw + sizeof(void *);
    aligned = (raw_addr + align - 1) & ~(uintptr_t)(align - 1);
    ((void **)aligned)[-1] = raw;
    return (void *)aligned;
}

void mmpm_database_aligned_free(void *ptr)
{
    if (ptr != NULL) {
        mmpm_database_free(((void **)ptr)[-1]);
    }
}

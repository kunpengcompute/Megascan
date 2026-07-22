/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "demo_common.h"

#include <stdlib.h>
#include <stdio.h>

#include "ms_allocator.h"
#include "ms.h"

static size_t g_allocs;
static size_t g_callocs;
static size_t g_frees;

static const char *const g_runtime_config_apis[] = {
    "ms_set_allocator",
    "ms_set_database_allocator",
    "ms_set_scratch_allocator",
    "ms_set_stream_allocator",
    "ms_set_misc_allocator",
    "ms_create_database",
    "ms_free_database",
    NULL
};

const demo_scenario_t g_demo_scenario_runtime_config = {
    "runtime-config",
    "运行时配置",
    "配置自定义内存分配器，创建数据库，然后恢复默认配置。",
    g_runtime_config_apis,
    demo_allocator_apis
};

static void *demo_alloc(size_t size)
{
    g_allocs++;
    return malloc(size);
}

static void *demo_calloc(size_t nmemb, size_t size)
{
    g_callocs++;
    return calloc(nmemb, size);
}

static void demo_free(void *ptr)
{
    g_frees++;
    free(ptr);
}

void demo_allocator_apis(void)
{
    ms_database_t *db = NULL;

    demo_section("运行时配置");

    (void)demo_step_ret("设置全部自定义内存分配器",
                        ms_set_allocator(demo_alloc, demo_calloc, demo_free));
    (void)demo_step_ret("设置 database 内存分配器",
                        ms_set_database_allocator(demo_alloc, demo_calloc, demo_free));
    (void)demo_step_ret("设置 scratch 内存分配器",
                        ms_set_scratch_allocator(demo_alloc, demo_calloc, demo_free));
    (void)demo_step_ret("设置 stream 内存分配器",
                        ms_set_stream_allocator(demo_alloc, demo_calloc, demo_free));
    (void)demo_step_ret("设置 misc 内存分配器",
                        ms_set_misc_allocator(demo_alloc, demo_calloc, demo_free));
    if (demo_step_ret("使用自定义分配器创建数据库",
                      ms_create_database(&db, MS_MATCHER_TYPE_HASH, 8, 4)) == MS_SUCCESS) {
        (void)demo_step_ret("释放数据库", ms_free_database(db));
    }

    (void)demo_step_ret("恢复 database 默认分配器", ms_set_database_allocator(NULL, NULL, NULL));
    (void)demo_step_ret("恢复 scratch 默认分配器", ms_set_scratch_allocator(NULL, NULL, NULL));
    (void)demo_step_ret("恢复 stream 默认分配器", ms_set_stream_allocator(NULL, NULL, NULL));
    (void)demo_step_ret("恢复 misc 默认分配器", ms_set_misc_allocator(NULL, NULL, NULL));
    (void)demo_step_ret("恢复全部默认分配器", ms_set_allocator(NULL, NULL, NULL));

    printf("%-48s malloc次数=%zu calloc次数=%zu free次数=%zu\n",
           "自定义分配器计数", g_allocs, g_callocs, g_frees);
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <pthread.h>
#include "ms_hash.h"
#include "ms_ac.h"
#include "ms_common.h"
#include "ms_allocator.h"
#include "ms_expr.h"

#define HASH_DEFAULT_BUCKET_SIZE 100000
#define IPV4_TUPLE_SIZE          16

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

__attribute__((visibility("default")))
int ms_handler_init(ms_handler_t *handler, ms_matcher_type type, uint32_t buckets, uint32_t secondary_buckets)
{
    int ret = 0;

    if (handler == NULL || type >= MS_MATCHER_TYPE_MAX)
        return MS_INVALID_PARA;

    if (handler->ms_matcher != NULL) {
        return MS_BUILT_ALREADY;
    }

    switch (type) {
        case MS_MATCHER_TYPE_HASH:
            buckets = buckets ? buckets : HASH_DEFAULT_BUCKET_SIZE;
            ret = hash_table_create((ms_hash **)&(handler->ms_matcher), buckets, IPV4_TUPLE_SIZE);
            if (ret)
                return ret;
            ret = hash_table_build(handler->ms_matcher, secondary_buckets);
            if (ret) {
                (void)hash_table_destroy(handler->ms_matcher);
                handler->ms_matcher = NULL;
                return ret;
            }
            break;
        case MS_MATCHER_TYPE_AC:
            handler->ms_matcher = ms_database_malloc(sizeof(ACAutomaton));
            if (handler->ms_matcher == NULL)
                return MS_MALLOC_FAIL;
            ret = ac_automaton_init(handler->ms_matcher);
            break;
        default:
            break;
    }

    if (ret) {
        ms_database_free(handler->ms_matcher);
        handler->ms_matcher = NULL;
    }
    handler->type = type;
    handler->is_built = false;

    return ret;
}

__attribute__((visibility("default")))
int ms_handler_destroy(ms_handler_t *handler)
{
    pthread_mutex_lock(&mutex);
    if (handler == NULL) {
        pthread_mutex_unlock(&mutex);
        return MS_INVALID_PARA;
    }

    ms_compile_context_destroy(handler);

    if (handler->ms_matcher == NULL) {
        pthread_mutex_unlock(&mutex);
        return MS_SUCCESS;
    }

    switch (handler->type) {
        case MS_MATCHER_TYPE_HASH:
            (void)hash_table_destroy(handler->ms_matcher);
            break;
        case MS_MATCHER_TYPE_AC:
            (void)ac_automaton_destroy(handler->ms_matcher);
            ms_database_free(handler->ms_matcher);
            break;
        default:
            pthread_mutex_unlock(&mutex);
            return MS_INVALID_PARA;
    }
    handler->ms_matcher = NULL;
    handler->type = MS_MATCHER_NONE;
    handler->is_built = false;
    pthread_mutex_unlock(&mutex);
    return MS_SUCCESS;
}

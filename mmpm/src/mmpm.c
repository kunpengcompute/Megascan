/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <pthread.h>
#include "mmpm_hash.h"
#include "mmpm_ac.h"
#include "mmpm_common.h"
#include "mmpm_allocator.h"
#include "mmpm_expr.h"

#define HASH_DEFAULT_BUCKET_SIZE 100000
#define IPV4_TUPLE_SIZE          16

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

__attribute__((visibility("default")))
int mmpm_handler_init(mmpm_handler_t *handler, mmpm_matcher_type type, uint32_t buckets, uint32_t secondary_buckets)
{
    int ret = 0;

    if (handler == NULL || type >= MMPM_MATCHER_TYPE_MAX)
        return MMPM_INVALID_PARA;

    if (handler->mmpm_matcher != NULL) {
        return MMPM_BUILT_ALREADY;
    }

    switch (type) {
        case MMPM_MATCHER_TYPE_HASH:
            buckets = buckets ? buckets : HASH_DEFAULT_BUCKET_SIZE;
            ret = hash_table_create((mmpm_hash **)&(handler->mmpm_matcher), buckets, IPV4_TUPLE_SIZE);
            if (ret)
                return ret;
            ret = hash_table_build(handler->mmpm_matcher, secondary_buckets);
            break;
        case MMPM_MATCHER_TYPE_AC:
            handler->mmpm_matcher = mmpm_malloc(sizeof(ACAutomaton));
            if (handler->mmpm_matcher == NULL)
                return MMPM_MALLOC_FAIL;
            ret = ac_automaton_init(handler->mmpm_matcher);
            break;
        default:
            break;
    }

    if (ret) {
        mmpm_free(handler->mmpm_matcher);
        handler->mmpm_matcher = NULL;
    }
    handler->type = type;
    handler->is_built = false;

    return ret;
}

__attribute__((visibility("default")))
int mmpm_handler_destroy(mmpm_handler_t *handler)
{
    pthread_mutex_lock(&mutex);
    if (handler == NULL) {
        pthread_mutex_unlock(&mutex);
        return MMPM_INVALID_PARA;
    }

    mmpm_compile_context_destroy(handler);

    if (handler->mmpm_matcher == NULL) {
        pthread_mutex_unlock(&mutex);
        return MMPM_SUCCESS;
    }

    switch (handler->type) {
        case MMPM_MATCHER_TYPE_HASH:
            (void)hash_table_destroy(handler->mmpm_matcher);
            break;
        case MMPM_MATCHER_TYPE_AC:
            (void)ac_automaton_destroy(handler->mmpm_matcher);
            mmpm_free(handler->mmpm_matcher);
            break;
        default:
            pthread_mutex_unlock(&mutex);
            return MMPM_INVALID_PARA;
    }
    handler->mmpm_matcher = NULL;
    handler->type = MMPM_MATCHER_NONE;
    handler->is_built = false;
    pthread_mutex_unlock(&mutex);
    return MMPM_SUCCESS;
}

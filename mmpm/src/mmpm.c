/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "mmpm_hash.h"
#include "mmpm_ac.h"
#include "mmpm.h"
#include "mmpm_allocator.h"

#define HASH_DEFAULT_BUCKET_SIZE 100000
#define IPV4_TUPLE_SIZE          16

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct ms_stream {
    ACAutomaton *automaton;
    uint32_t node_index;
    uint64_t offset;
    uint64_t generation;
};

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

static int mmpm_handler_is_valid(mmpm_handler_t *handler, bool is_add)
{
    if (handler == NULL)
        return MMPM_INVALID_PARA;

    if (handler->mmpm_matcher && handler->type < MMPM_MATCHER_TYPE_MAX)
        return MMPM_SUCCESS;

    if (is_add && handler->mmpm_matcher == NULL && handler->type == MMPM_MATCHER_NONE)
        return MMPM_SUCCESS;

    return MMPM_INVALID_PARA;
}

__attribute__((visibility("default")))
int mmpm_add_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;

    if (mmpm_handler_is_valid(handler, true) || rule == NULL)
        return MMPM_INVALID_PARA;

    if (handler->mmpm_matcher == NULL && handler->type == MMPM_MATCHER_NONE) {
        printf("first import the rules need choose hash or ac.");
        printf("\n");
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MMPM_MATCHER_TYPE_HASH:
                ret = hash_table_add(handler->mmpm_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                /* Considering the possibility of batch insertion in hash algorithms, directly deleting the entire hash table when an exception occurs in a batch insertion is not friendly. 
                Here, only an exception prompt is returned, and the successfully inserted part is not considered a memory leak. */
                if (ret)
                    return ret;
                break;
            case MMPM_MATCHER_TYPE_AC:
                if (handler->is_built)
                    return MMPM_BUILT_ALREADY;
                ret = ac_automaton_insert(handler->mmpm_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    ac_automaton_destroy(handler->mmpm_matcher);
                    return ret;
                }
                break;
            default:
                /* no possible. */
                break;
        }
    }

    if (handler->type == MMPM_MATCHER_TYPE_AC) {
        ret = ac_automaton_build(handler->mmpm_matcher);
        if (ret)
            (void)ac_automaton_destroy(handler->mmpm_matcher);
        else
            handler->is_built = true;
    }

    return ret;
}

__attribute__((visibility("default")))
int mmpm_find_rule(mmpm_handler_t *handler, mmpm_rule_t *rule)
{
    int ret = MMPM_SUCCESS;

    if (rule == NULL || rule->key == NULL || mmpm_handler_is_valid(handler, false))
        return MMPM_INVALID_PARA;

    switch (handler->type) {
        case MMPM_MATCHER_TYPE_HASH:
            ret = hash_table_find(handler->mmpm_matcher, rule->key, rule->key_len, &(rule->alias));
            break;
        case MMPM_MATCHER_TYPE_AC:
            if (!handler->is_built) {
                return MMPM_INVALID_PARA;
            }
            ret = ac_automaton_match(handler->mmpm_matcher, rule->key, rule->key_len);
            break;
        default:
            /* no possible. */
            break;
    }

    return ret;
}

__attribute__((visibility("default")))
int mmpm_remove_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;

    if (rule == NULL || mmpm_handler_is_valid(handler, false))
        return MMPM_INVALID_PARA;

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MMPM_MATCHER_TYPE_HASH:
                ret = hash_table_remove(handler->mmpm_matcher, rule[i].key, rule[i].key_len);
                if (ret)
                    goto end;
                break;
            case MMPM_MATCHER_TYPE_AC:
                if (!handler->is_built) {
                    ret = MMPM_INVALID_PARA;
                    goto end;
                }
                ret = ac_automaton_remove(handler->mmpm_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret)
                    goto end;
                break;
            default:
                /* no possible. */
                break;
        }
    }

end:
    return ret;
}

__attribute__((visibility("default")))
int mmpm_set_hash_func(mmpm_handler_t *handler, hash_calc_func hash_func)
{
    if (handler == NULL || handler->mmpm_matcher == NULL || handler->type != MMPM_MATCHER_TYPE_HASH || hash_func == NULL)
        return MMPM_INVALID_PARA;
    
    hash_table_set_hash_func(handler->mmpm_matcher, hash_func);

    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_open_stream(mmpm_handler_t *handler, ms_stream_t **stream)
{
    if (handler == NULL || stream == NULL || *stream != NULL) {
        return MMPM_INVALID_PARA;
    }

    if (handler->type != MMPM_MATCHER_TYPE_AC ||
        handler->mmpm_matcher == NULL ||
        !handler->is_built) {
        return MMPM_INVALID_PARA;
    }

    ms_stream_t *s = (ms_stream_t *)calloc(1, sizeof(ms_stream_t));
    if (s == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    ACAutomaton *automaton = (ACAutomaton *)handler->mmpm_matcher;
    s->automaton = automaton;
    s->node_index = 0;
    s->offset = 0;
    s->generation = automaton->generation;

    *stream = s;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_scan_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    if (stream == NULL || stream->automaton == NULL || data == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MMPM_STREAM_STALE;
    }

    int ret = ac_automaton_match_state(stream->automaton,
                                       data,
                                       len,
                                       &stream->node_index);
    if (ret == MMPM_SUCCESS || ret == MMPM_FIND_FAIL) {
        stream->offset += len;
    }

    return ret;
}

__attribute__((visibility("default")))
int mmpm_reset_stream(ms_stream_t *stream)
{
    if (stream == NULL || stream->automaton == NULL) {
        return MMPM_INVALID_PARA;
    }

    stream->node_index = 0;
    stream->offset = 0;
    stream->generation = stream->automaton->generation;

    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int mmpm_close_stream(ms_stream_t *stream)
{
    if (stream == NULL) {
        return MMPM_INVALID_PARA;
    }

    free(stream);
    return MMPM_SUCCESS;
}
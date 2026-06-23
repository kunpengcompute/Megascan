/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "ms_hash.h"
#include "ms_ac.h"
#include "ms.h"
#include "ms_allocator.h"

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
            break;
        case MS_MATCHER_TYPE_AC:
            handler->ms_matcher = ms_malloc(sizeof(ACAutomaton));
            if (handler->ms_matcher == NULL)
                return MS_MALLOC_FAIL;
            ret = ac_automaton_init(handler->ms_matcher);
            break;
        default:
            break;
    }

    if (ret) {
        ms_free(handler->ms_matcher);
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
            ms_free(handler->ms_matcher);
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

static int ms_handler_is_valid(ms_handler_t *handler, bool is_add)
{
    if (handler == NULL)
        return MS_INVALID_PARA;

    if (handler->ms_matcher && handler->type < MS_MATCHER_TYPE_MAX)
        return MS_SUCCESS;

    if (is_add && handler->ms_matcher == NULL && handler->type == MS_MATCHER_NONE)
        return MS_SUCCESS;

    return MS_INVALID_PARA;
}

__attribute__((visibility("default")))
int ms_add_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;

    if (ms_handler_is_valid(handler, true) || rule == NULL)
        return MS_INVALID_PARA;

    if (handler->ms_matcher == NULL && handler->type == MS_MATCHER_NONE) {
        printf("first import the rules need choose hash or ac.");
        printf("\n");
        return MS_INVALID_PARA;
    }

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MS_MATCHER_TYPE_HASH:
                ret = hash_table_add(handler->ms_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                /* Considering the possibility of batch insertion in hash algorithms, directly deleting the entire hash table when an exception occurs in a batch insertion is not friendly. 
                Here, only an exception prompt is returned, and the successfully inserted part is not considered a memory leak. */
                if (ret)
                    return ret;
                break;
            case MS_MATCHER_TYPE_AC:
                if (handler->is_built)
                    return MS_BUILT_ALREADY;
                ret = ac_automaton_insert(handler->ms_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
                if (ret) {
                    ac_automaton_destroy(handler->ms_matcher);
                    return ret;
                }
                break;
            default:
                /* no possible. */
                break;
        }
    }

    if (handler->type == MS_MATCHER_TYPE_AC) {
        ret = ac_automaton_build(handler->ms_matcher);
        if (ret)
            (void)ac_automaton_destroy(handler->ms_matcher);
        else
            handler->is_built = true;
    }

    return ret;
}

__attribute__((visibility("default")))
int ms_find_rule(ms_handler_t *handler, ms_rule_t *rule)
{
    int ret = MS_SUCCESS;

    if (rule == NULL || rule->key == NULL || ms_handler_is_valid(handler, false))
        return MS_INVALID_PARA;

    switch (handler->type) {
        case MS_MATCHER_TYPE_HASH:
            ret = hash_table_find(handler->ms_matcher, rule->key, rule->key_len, &(rule->alias));
            break;
        case MS_MATCHER_TYPE_AC:
            if (!handler->is_built) {
                return MS_INVALID_PARA;
            }
            ret = ac_automaton_match(handler->ms_matcher, rule->key, rule->key_len);
            break;
        default:
            /* no possible. */
            break;
    }

    return ret;
}

__attribute__((visibility("default")))
int ms_remove_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num)
{
    uint32_t i;
    int ret = 0;

    if (rule == NULL || ms_handler_is_valid(handler, false))
        return MS_INVALID_PARA;

    for (i = 0; i < rule_num; i++) {
        switch (handler->type) {
            case MS_MATCHER_TYPE_HASH:
                ret = hash_table_remove(handler->ms_matcher, rule[i].key, rule[i].key_len);
                if (ret)
                    goto end;
                break;
            case MS_MATCHER_TYPE_AC:
                if (!handler->is_built) {
                    ret = MS_INVALID_PARA;
                    goto end;
                }
                ret = ac_automaton_remove(handler->ms_matcher, rule[i].alias, rule[i].key, rule[i].key_len);
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
int ms_set_hash_func(ms_handler_t *handler, hash_calc_func hash_func)
{
    if (handler == NULL || handler->ms_matcher == NULL || handler->type != MS_MATCHER_TYPE_HASH || hash_func == NULL)
        return MS_INVALID_PARA;
    
    hash_table_set_hash_func(handler->ms_matcher, hash_func);

    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_open_stream(ms_handler_t *handler, ms_stream_t **stream)
{
    if (handler == NULL || stream == NULL || *stream != NULL) {
        return MS_INVALID_PARA;
    }

    if (handler->type != MS_MATCHER_TYPE_AC ||
        handler->ms_matcher == NULL ||
        !handler->is_built) {
        return MS_INVALID_PARA;
    }

    ms_stream_t *s = (ms_stream_t *)ms_calloc(1, sizeof(ms_stream_t));
    if (s == NULL) {
        return MS_MALLOC_FAIL;
    }

    ACAutomaton *automaton = (ACAutomaton *)handler->ms_matcher;
    s->automaton = automaton;
    s->node_index = 0;
    s->offset = 0;
    s->generation = automaton->generation;

    *stream = s;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_scan_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len)
{
    if (stream == NULL || stream->automaton == NULL || data == NULL) {
        return MS_INVALID_PARA;
    }

    if (stream->generation != stream->automaton->generation) {
        return MS_STREAM_STALE;
    }

    int ret = ac_automaton_match_state(stream->automaton,
                                       data,
                                       len,
                                       &stream->node_index);
    if (ret == MS_SUCCESS || ret == MS_FIND_FAIL) {
        stream->offset += len;
    }

    return ret;
}

__attribute__((visibility("default")))
int ms_reset_stream(ms_stream_t *stream)
{
    if (stream == NULL || stream->automaton == NULL) {
        return MS_INVALID_PARA;
    }

    stream->node_index = 0;
    stream->offset = 0;
    stream->generation = stream->automaton->generation;

    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_close_stream(ms_stream_t *stream)
{
    if (stream == NULL) {
        return MS_INVALID_PARA;
    }

    ms_free(stream);
    return MS_SUCCESS;
}

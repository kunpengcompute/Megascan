/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.

 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "ms_hash.h"
#include "ms_allocator.h"

#define ALIGN(s) (((s) + 63) & ~63)
#define ALIGN_SIZE      64

#define DEFAULT_SUBBUCKET_NUM   96
#define DJB2_HASH_SEED  5381
#define DJB2_HASH_SHIFT 5
#define UINT32_HALF_SHIFT   16

static uint32_t hash_func_djb2_hash32(const uint8_t *key, uint32_t key_len)
{
    uint32_t hash = DJB2_HASH_SEED;

    for (uint32_t i = 0; i < key_len; i++) {
        hash = ((hash << DJB2_HASH_SHIFT) + hash) + key[i];
    }

    return hash;
}

int hash_table_create(ms_hash **hash_table, uint32_t buckets, uint16_t key_size)
{
    int ret;
    ms_hash *table = NULL;

    if (hash_table == NULL || buckets == 0) {
        return MS_INVALID_PARA;
    }

    *hash_table = NULL;

    ret = posix_memalign((void **)(&(table)), ALIGN_SIZE, sizeof(ms_hash));
    if (ret || table == NULL) {
        return MS_MALLOC_FAIL;
    }

    memset(table, 0, sizeof(ms_hash));
    table->bucket_num = buckets;
    table->key_size = key_size;
    table->build_ok = 0;

    *hash_table = table;

    return 0;
}

int hash_table_build(ms_hash *hash_table, uint32_t secondary_buckets)
{
    uint64_t buckets_cnt, subbuckets_cnt;
    size_t malloc_size = 0;
    uint8_t *malloc_ptr = NULL;
    uint32_t i, j;
    int ret;

    if (hash_table == NULL) {
        return MS_INVALID_PARA;
    }

    if (hash_table->build_ok) {
        return MS_BUILT_ALREADY;
    }

    buckets_cnt = hash_table->bucket_num;
    subbuckets_cnt = secondary_buckets ? secondary_buckets : DEFAULT_SUBBUCKET_NUM;

    malloc_size += ALIGN(sizeof(ms_hash_bucket) * buckets_cnt);
    malloc_size += ALIGN(sizeof(ms_hash_subbucket) * buckets_cnt * subbuckets_cnt);

    ret = posix_memalign((void **)(&malloc_ptr), ALIGN_SIZE, malloc_size);
    if (ret || malloc_ptr == NULL) {
        return MS_MALLOC_FAIL;
    }

    memset(malloc_ptr, 0, malloc_size);
    hash_table->buckets = (ms_hash_bucket *)malloc_ptr;

    malloc_ptr += ALIGN(sizeof(ms_hash_bucket) * buckets_cnt);
    for (i = 0; i < buckets_cnt; i++) {
        hash_table->buckets[i].subbuckets = \
            (ms_hash_subbucket *)(malloc_ptr + i * (sizeof(ms_hash_subbucket) * subbuckets_cnt));
        hash_table->buckets[i].subbucket_num = subbuckets_cnt;
        for (j = 0; j < subbuckets_cnt; j++) {
            ret = pthread_rwlock_init(&hash_table->buckets[i].subbuckets[j].rwlock, NULL);
            if (ret) {
                ret = MS_INIT_RWLOCK_FAIL;
                goto error;
            }
        }
    }

    hash_table->hash_func = hash_func_djb2_hash32;
    hash_table->build_ok = 1;

    return 0;
error:
    for (uint32_t k = 0; k <= i; k++) {
        uint32_t max_sub = (k == i) ? j : hash_table->buckets[k].subbucket_num;
        for (uint32_t l = 0; l < max_sub; l++) {
            pthread_rwlock_destroy(&hash_table->buckets[k].subbuckets[l].rwlock);
        }
    }
    ms_free(hash_table->buckets);
    hash_table->buckets = NULL;
    return ret;
}

static ms_hash_node *hash_table_match_node(ms_hash_node *node, const uint8_t *key, uint32_t key_len)
{
    while (node) {
        if (key_len != node->key_len || memcmp(key, node->content, key_len) != 0) {
            node = node->link_next;
            continue;
        }
        return node;
    }
    return NULL;
}

static uint32_t secondary_hash(uint32_t key)
{
    key ^= (key >> UINT32_HALF_SHIFT) | (key << UINT32_HALF_SHIFT);
    return key;
}

int hash_table_add(ms_hash *hash_table, uint32_t alias, const uint8_t *key, uint32_t key_len)
{
    uint32_t hash, bucket_idx, subbucket_idx;
    ms_hash_node *node = NULL;
    int ret;

    if (hash_table == NULL || key == NULL || key_len == 0 || key_len > hash_table->key_size) {
        return MS_INVALID_PARA;
    }
    
    hash = hash_table->hash_func(key, key_len);
    bucket_idx = hash % hash_table->bucket_num;
    subbucket_idx = secondary_hash(hash) % hash_table->buckets[bucket_idx].subbucket_num;

    pthread_rwlock_rdlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);
    node = hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes;
    node = hash_table_match_node(node, key, key_len);
    if (node) {
        node->alias = alias;
        pthread_rwlock_unlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);
        return MS_SUCCESS;
    }
    pthread_rwlock_unlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);

    ret = posix_memalign((void **)(&node), ALIGN_SIZE, sizeof(ms_hash_node) + hash_table->key_size);
    if (ret || node == NULL) {
        return MS_MALLOC_FAIL;
    }

    pthread_rwlock_wrlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);
    memset(node, 0, sizeof(ms_hash_node) + hash_table->key_size);
    node->bucket_index = bucket_idx;
    node->subbucket_index = subbucket_idx;
    node->link_next = hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes;

    if (hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes != NULL)
        hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes->link_pre = node;
    hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes = node;
    memcpy(node->content, key, key_len);
    node->key_len = key_len;
    node->alias = alias;

    hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].node_num++;
    pthread_rwlock_unlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);

    return 0;
}

int hash_table_find(ms_hash *hash_table, const uint8_t *key, uint32_t key_len, uint32_t *alias)
{
    uint32_t hash, bucket_idx, subbucket_idx;
    ms_hash_node *node = NULL;

    if (hash_table == NULL || alias  == NULL) {
        return MS_INVALID_PARA;
    }

    if (key == NULL || key_len == 0) {
        return MS_FIND_FAIL;
    }

    hash = hash_table->hash_func(key, key_len);
    bucket_idx = hash % hash_table->bucket_num;
    subbucket_idx = secondary_hash(hash) % hash_table->buckets[bucket_idx].subbucket_num;
    if (!hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes) {
        return MS_FIND_FAIL;
    }

    pthread_rwlock_rdlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);
    node = hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes;
    node = hash_table_match_node(node, key, key_len);
    if (node) {
        *alias = node->alias;
        pthread_rwlock_unlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);
        return 0;
    }
    pthread_rwlock_unlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);
    return MS_FIND_FAIL;
}

int hash_table_remove(ms_hash *hash_table, const uint8_t *key, uint32_t key_len)
{
    uint32_t hash, bucket_idx, subbucket_idx;
    ms_hash_node *node = NULL;

    if (hash_table == NULL) {
        return MS_INVALID_PARA;
    }

    if (key == NULL || key_len == 0) {
        goto end;
    }

    hash = hash_table->hash_func(key, key_len);
    bucket_idx = hash % hash_table->bucket_num;
    subbucket_idx = secondary_hash(hash) % hash_table->buckets[bucket_idx].subbucket_num;

    pthread_rwlock_wrlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);
    node = hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes;
    node = hash_table_match_node(node, key, key_len);
    if (node) {
        if (node->link_pre)
            node->link_pre->link_next = node->link_next;
        else
            hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].nodes = node->link_next;
        
        if (node->link_next)
            node->link_next->link_pre = node->link_pre;
        ms_free(node);
        hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].node_num--;
    }
    pthread_rwlock_unlock(&hash_table->buckets[bucket_idx].subbuckets[subbucket_idx].rwlock);

end:
    return MS_SUCCESS;
}

int hash_table_destroy(ms_hash *hash_table)
{
    uint32_t i, j;

    if (hash_table == NULL) {
        return MS_INVALID_PARA;
    }

    /* free all rwlock of stage-2 hash_bucket. */
    for (i = 0; i < hash_table->bucket_num; i++) {
        for (j = 0; j < hash_table->buckets[i].subbucket_num; j++) {
            (void)pthread_rwlock_wrlock(&hash_table->buckets[i].subbuckets[j].rwlock);
            ms_hash_node* subbucket_head = hash_table->buckets[i].subbuckets[j].nodes;
            ms_hash_node* tmp;
            while (subbucket_head) {
                tmp = subbucket_head;
                subbucket_head = subbucket_head->link_next;
                ms_free(tmp);
            }
            (void)pthread_rwlock_unlock(&hash_table->buckets[i].subbuckets[j].rwlock);
            (void)pthread_rwlock_destroy(&hash_table->buckets[i].subbuckets[j].rwlock);
        }
    }

    if (hash_table->buckets) {
        ms_free(hash_table->buckets);
        hash_table->buckets = NULL;
    }

    ms_free(hash_table);
    return MS_SUCCESS;
}

void hash_table_set_hash_func(ms_hash *hash_table, hash_calc_func hash_func)
{
    if (hash_table == NULL) {
        return;
    }

    hash_table->hash_func = hash_func;
    return;
}
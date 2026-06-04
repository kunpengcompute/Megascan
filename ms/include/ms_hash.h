/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_HASH_H
#define MS_HASH_H
#include <stdint.h>
#include <pthread.h>
#include "ms.h"

typedef struct __ms_hash_node {
    struct __ms_hash_node *link_pre, *link_next;
    uint32_t bucket_index;
    uint16_t subbucket_index;
    uint32_t alias;
    uint16_t key_len;
    uint8_t content[0];
} ms_hash_node;

typedef struct __ms_hash_subbucket {
    ms_hash_node *nodes;
    uint32_t node_num;
    pthread_rwlock_t rwlock;
} ms_hash_subbucket;

typedef struct __ms_hash_bucket {
    ms_hash_subbucket *subbuckets;
    uint32_t subbucket_num;
} ms_hash_bucket;

typedef struct __ms_hash {
    uint32_t bucket_num;
    uint16_t key_size;

    ms_hash_bucket *buckets;

    hash_calc_func hash_func;

    int build_ok;
} ms_hash;

int hash_table_create(ms_hash **hash_table, uint32_t buckets, uint16_t key_size);
int hash_table_build(ms_hash *hash_table, uint32_t secondary_buckets);
int hash_table_add(ms_hash *hash_table, uint32_t alias, const uint8_t *key, uint32_t key_len);
int hash_table_find(ms_hash *hash_table, const uint8_t *key, uint32_t key_len, uint32_t *alias);
int hash_table_remove(ms_hash *hash_table, const uint8_t *key, uint32_t key_len);
int hash_table_destroy(ms_hash *hash_table);
void hash_table_set_hash_func(ms_hash *hash_table, hash_calc_func hash_func);

#endif

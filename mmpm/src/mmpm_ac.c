/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include "securec.h"
#include "mmpm_ac.h"

#define MMPM_AC_AUTOMATON_INITIAL_ID 1

static void *secure_realloc(void *old_ptr, size_t old_size, size_t new_size)
{
    void *new_ptr = NULL;
    errno_t ret = EOK;

    if (new_size == 0) {
        goto end;
    }

    if ((old_ptr == NULL && old_size != 0) || (old_ptr != NULL && old_size == 0)) {
        goto end;
    }

    new_ptr = malloc(new_size);
    if (new_ptr == NULL) {
        goto end;
    }

    if (old_size == 0) {
        goto end;
    }

    ret = memcpy_s(new_ptr, new_size, old_ptr, (new_size < old_size) ? new_size : old_size);
    if (ret != EOK) {
        free(new_ptr);
        new_ptr = NULL;
        goto end;
    }
    free(old_ptr);

end:
    return new_ptr;
}

static ACNode *create_ac_node()
{
    ACNode *node = (ACNode *)calloc(1, sizeof(ACNode));

    return node;
}

int ac_automaton_init(ACAutomaton *automaton)
{
    if (automaton == NULL)
        return MMPM_INVALID_PARA;

    automaton->root = create_ac_node();
    if (automaton->root == NULL)
        return MMPM_MALLOC_FAIL;

    automaton->root->node_idx = 0;  
    automaton->pattern_count = 0;
    automaton->id_count = MMPM_AC_AUTOMATON_INITIAL_ID;
    automaton->table = NULL;
    automaton->output = NULL;
    return 0;
}

static void do_ac_node_free(ACNode *node)
{
    uint32_t i;

    if (node == NULL) {
        return;
    }

    for (i = 0; i < BYTE_MAX_SIZE; i++) {
        if (node->flags[i] == false)
            do_ac_node_free(node->children[i]);
    }
    if (node->id_set != NULL) {
        free(node->id_set);
    }

    memset_s(node, sizeof(ACNode), 0, sizeof(ACNode));
    free(node);
}

int ac_automaton_destroy(ACAutomaton *automaton)
{
    if (automaton == NULL)
        return MMPM_INVALID_PARA;
    free(automaton->table);
    if ((automaton->id_count > MMPM_AC_AUTOMATON_INITIAL_ID) && (automaton->output != NULL)) {
        for (uint32_t i = 0; i < automaton->id_count; ++i) {
            if (automaton->output[i]) {
                free(automaton->output[i]);
            }
        }
    }

    if (automaton->output != NULL) {
        free(automaton->output);
    }

    automaton->table = NULL;
    automaton->output = NULL;
    automaton->root = NULL;
    automaton->pattern_count = 0;
    automaton->id_count = 0;

    return MMPM_SUCCESS;
}

int ac_automaton_insert(ACAutomaton *automaton, uint32_t alias, const uint8_t *pattern, uint32_t len)
{
    ACNode *node = automaton->root;
    int *ptr = NULL;

    if (automaton->root == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (uint32_t i = 0; i < len; i++) {
        if (node->children[pattern[i]] == NULL) {
            node->children[pattern[i]] = create_ac_node();
            if (node->children[pattern[i]] == NULL) {
                automaton->id_count = 0;
                return MMPM_MALLOC_FAIL;
            }
            node->children[pattern[i]]->node_idx = automaton->id_count;
            automaton->id_count++;
        }
        node = node->children[pattern[i]];
    }

    if (node->id_size == INT_MAX) {
        return MMPM_MALLOC_FAIL;
    }
    ptr = secure_realloc(node->id_set, node->id_size * sizeof(int), (node->id_size + 1) * sizeof(int));
    if (ptr == NULL)
        return MMPM_MALLOC_FAIL;
    node->id_set = ptr;

    node->id_set[node->id_size++] = (int)alias;
    automaton->pattern_count++;

    return 0;
}

static int *merge_arrays(const int *a, int a_size, const int *b, int b_size, int *out_size)
{
    int *res = NULL, *ptr = NULL;
    int i = 0, j = 0, k = 0;
    int last = -1, cnt = 0;
    if ((a == NULL) && (b == NULL)) {
        goto end;
    }
    int size_sum = a_size + b_size;
    if (size_sum == 0) {
        goto end;
    }

    if (SIZE_MAX / size_sum < sizeof(int)) {
        goto end;
    }

    res = (int *)malloc(size_sum * sizeof(int));
    if (res == NULL) {
        goto end;
    }
    
    while (i < a_size && j < b_size) {
        if (a[i] < b[j]) {
            res[k++] = a[i++];
        }
        else {
            res[k++] = b[j++];
        }
    }

    while (i < a_size) {
        res[k++] = a[i++];
    }
    while (j < b_size) {
        res[k++] = b[j++];
    }

    // deduplicated data
    for (int m = 0; m < k; m++) {
        if (m == 0 || res[m] != last) {
            res[cnt++] = res[m];
            last = res[m];
        }
    }

    if (cnt == k) {
        goto end;
    }

    ptr = secure_realloc(res, k * sizeof(int), cnt * sizeof(int));
    if (ptr == NULL) {
        free(res);
        res = NULL;
        cnt = 0;
        goto end;
    }
    res = ptr;

end:
    *out_size = cnt;
    return res;
}

static int node_output_integrate(ACNode *node)
{
    int merged_size = 0;
    int *merged = NULL;

    if (node == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (node->id_set == 0 && node->fail->out_size == 0) {
        return MMPM_SUCCESS;
    }

    merged = merge_arrays(node->id_set, node->id_size, node->fail->output, node->fail->out_size, &merged_size);
    if (merged == NULL) {
        return MMPM_MALLOC_FAIL;
    }
    
    if (node->output) {
        free(node->output);
    }

    node->output = merged;
    node->out_size = merged_size;

    return MMPM_SUCCESS;
}

static int node_output_integrate_for_root_child(ACNode *node)
{
    int merged_size = 0;
    int *merged = merge_arrays(node->id_set, node->id_size, NULL, 0, &merged_size);
    if (merged == NULL) {
        return MMPM_MALLOC_FAIL;
    }
    if (node->output) {
        free(node->output);
    }
    node->output = merged;
    node->out_size = merged_size;
    return MMPM_SUCCESS;
}

static int do_ac_table_build(uint32_t *table, int **output, ACNode *node, uint32_t id_count)
{
    uint32_t i;

    if (node == NULL) {
        return MMPM_SUCCESS;
    }

    if (node->node_idx >= id_count) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < BYTE_MAX_SIZE; i++) {
        if (node->children[i]->node_idx >= id_count) {
            return MMPM_INVALID_PARA;
        }
        table[(size_t)node->node_idx * BYTE_MAX_SIZE + i] = node->children[i]->node_idx;
    }

    output[node->node_idx] = node->output;

    for (i = 0; i < BYTE_MAX_SIZE; i++) {
        if (node->flags[i] == false) {
            do_ac_table_build(table, output, node->children[i], id_count);
        }
    }
    return MMPM_SUCCESS;
}

static int ac_table_build(ACAutomaton *automaton)
{
    if (automaton->id_count != 0 &&
        (SIZE_MAX / automaton->id_count) < BYTE_MAX_SIZE * sizeof(uint32_t)) {
        return MMPM_MALLOC_FAIL;
    }
    automaton->table = (uint32_t *)malloc(sizeof(uint32_t) * automaton->id_count * BYTE_MAX_SIZE);
    if (automaton->table == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    if (automaton->id_count != 0 && (SIZE_MAX / automaton->id_count) < sizeof(int *)) {
        return MMPM_MALLOC_FAIL;
    }
    automaton->output = (int **)malloc(sizeof(int *) * automaton->id_count);
    if (automaton->output == NULL) {
        return MMPM_MALLOC_FAIL;
    }
    memset_s(automaton->output, sizeof(int *) * automaton->id_count, 0, sizeof(int *) * automaton->id_count);

    // fill the change table content.
    int ret = do_ac_table_build(automaton->table, automaton->output, automaton->root, automaton->id_count);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }
    do_ac_node_free(automaton->root);
    automaton->root = NULL;

    return MMPM_SUCCESS;
}

int ac_automaton_build(ACAutomaton *automaton)
{
    if (automaton->root == NULL) {
        return MMPM_INVALID_PARA;
    }
    ACNode *root = automaton->root;
    ACNode *child, *fail;
    ACNode **queue = (ACNode **)calloc(automaton->id_count, sizeof(ACNode *));
    int front = 0, rear = 0;
    int ret = 0;
    if (queue == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    for (int i = 0; i < BYTE_MAX_SIZE; i++) {
        if ((uint32_t)rear >= automaton->id_count) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        if (root->children[i]) {
            if (root->children[i]->id_size > 0) {
                ret = node_output_integrate_for_root_child(root->children[i]);
                if (ret) {
                    goto end;
                }
            }
            root->children[i]->fail = root;
            queue[rear++] = root->children[i];
        } else {
            root->children[i] = root;
            root->flags[i] = true;
        }
    }

    while (front < rear) {
        if (((uint32_t)rear >= automaton->id_count) || ((uint32_t)front >= automaton->id_count)) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        ACNode *cur = queue[front++];

        for (int i = 0; i < BYTE_MAX_SIZE; i++) {
            if (cur->children[i] == NULL) {
                ACNode *tmp = cur;
                while (!tmp->children[i] && tmp != automaton->root) {
                    tmp = tmp->fail;
                }
                cur->children[i] = tmp->children[i] ? tmp->children[i] : root;
                cur->flags[i] = true;
                continue;
            }

            child = cur->children[i];
            fail = cur->fail;
            while (fail && fail->children[i] == NULL) {
                fail = fail->fail;
            }
            child->fail = fail ? fail->children[i] : root;

            ret = node_output_integrate(child);
            if (ret) {
                goto end;
            }

            queue[rear++] = child;
        }
    }
    /* Translate the pre-tree to query-table. */
    ret = ac_table_build(automaton);

end:
    if (queue != NULL) {
        free(queue);
    }

    return ret;
}

int ac_automaton_match(ACAutomaton *automaton, const uint8_t *data, uint32_t len)
{
    uint8_t idx;
    int table_ret = MMPM_FIND_FAIL;

    uint32_t node_index = 0;
    for (uint32_t i = 0; i < len; i++) {
        idx = data[i];
        node_index = automaton->table[(uint64_t)node_index * BYTE_MAX_SIZE + idx];
        if (automaton->output[node_index] != NULL)
            table_ret = MMPM_SUCCESS;
    }
    return table_ret;
}
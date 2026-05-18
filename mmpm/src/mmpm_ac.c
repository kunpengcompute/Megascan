/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include "mmpm_ac.h"

#define MMPM_AC_AUTOMATON_INITIAL_ID 1

static void *secure_realloc(void *old_ptr, size_t old_size, size_t new_size)
{
    void *new_ptr = NULL;
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

    memcpy(new_ptr, old_ptr, (new_size < old_size) ? new_size : old_size);
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
        if (node->is_non_child_node[i] == false)
            do_ac_node_free(node->children[i]);
    }
    if (node->id_set != NULL) {
        free(node->id_set);
    }
    if (node->output != NULL) {
        free(node->output);
    }

    memset(node, 0, sizeof(ACNode));
    free(node);
}

int ac_automaton_destroy(ACAutomaton *automaton)
{
    if (automaton == NULL)
        return MMPM_INVALID_PARA;
    free(automaton->table);
    if (automaton->output != NULL) {
        free(automaton->output);
    }

    do_ac_node_free(automaton->root);

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
            
            node->children[pattern[i]]->parent = node;
            node->children[pattern[i]]->parent_ch = pattern[i];
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
        if (node->is_non_child_node[i] == false) {
            int ret = do_ac_table_build(table, output, node->children[i], id_count);
            if (ret != MMPM_SUCCESS) {
                return ret;
            }
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
    memset(automaton->output, 0, sizeof(int *) * automaton->id_count);

    // fill the change table content.
    int ret = do_ac_table_build(automaton->table, automaton->output, automaton->root, automaton->id_count);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

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
            root->is_non_child_node[i] = true;
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
                cur->is_non_child_node[i] = true;
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

typedef struct {
    ACNode *node;
    ACNode *fail;
} DeletedNodeInfo;

static bool has_no_real_children(ACNode *node)
{
    for (int i = 0; i < BYTE_MAX_SIZE; i++) {
        if (node->children[i] != NULL && !node->is_non_child_node[i]) {
            return false;
        }
    }
    return true;
}

static void free_ac_node_simple(ACNode *node)
{
    if (node == NULL) {
        return;
    }

    if (node->id_set != NULL) {
        free(node->id_set);
    }
    if (node->output != NULL) {
        free(node->output);
    }

    memset(node, 0, sizeof(ACNode));
    free(node);
}

static int find_deleted_node_index(const DeletedNodeInfo *deleted_infos, uint32_t deleted_cnt, const ACNode *node)
{
    for (uint32_t i = 0; i < deleted_cnt; i++) {
        if (deleted_infos[i].node == node) {
            return (int)i;
        }
    }
    return -1;
}

static ACNode *resolve_deleted_fail_target(const DeletedNodeInfo *deleted_infos, uint32_t deleted_cnt,
    ACNode *deleted_target, ACNode *root)
{
    ACNode *target = deleted_target;
    int idx = find_deleted_node_index(deleted_infos, deleted_cnt, target);

    while (idx >= 0) {
        target = deleted_infos[idx].fail;
        if (target == NULL) {
            return root;
        }
        idx = find_deleted_node_index(deleted_infos, deleted_cnt, target);
    }

    return target;
}

static void prune_empty_nodes_collect(ACAutomaton *automaton, ACNode *node,
    DeletedNodeInfo *deleted_infos, uint32_t *deleted_cnt)
{
    while (node != NULL && node != automaton->root) {
        if (node->id_size != 0 || !has_no_real_children(node)) {
            break;
        }

        ACNode *parent = node->parent;
        uint8_t ch = node->parent_ch;

        deleted_infos[*deleted_cnt].node = node;
        deleted_infos[*deleted_cnt].fail = node->fail;
        (*deleted_cnt)++;

        parent->children[ch] = NULL;
        parent->is_non_child_node[ch] = false;

        free_ac_node_simple(node);
        node = parent;
    }
}

static int update_fail_pointers_local(ACAutomaton *automaton, const DeletedNodeInfo *deleted_infos, uint32_t deleted_cnt)
{
    if (deleted_cnt == 0) {
        return MMPM_SUCCESS;
    }

    ACNode **queue = (ACNode **)calloc(automaton->id_count, sizeof(ACNode *));
    if (queue == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    int front = 0, rear = 0;
    queue[rear++] = automaton->root;

    while (front < rear) {
        ACNode *cur = queue[front++];

        if (cur != automaton->root && cur->fail != NULL) {
            int idx = find_deleted_node_index(deleted_infos, deleted_cnt, cur->fail);
            if (idx >= 0) {
                cur->fail = resolve_deleted_fail_target(deleted_infos, deleted_cnt, cur->fail, automaton->root);
            }
        }

        for (int i = 0; i < BYTE_MAX_SIZE; i++) {
            if (cur->children[i] != NULL && !cur->is_non_child_node[i]) {
                queue[rear++] = cur->children[i];
            }
        }
    }

    free(queue);
    return MMPM_SUCCESS;
}

static int clear_virtual_edges_and_reindex(ACAutomaton *automaton)
{
    ACNode **queue = (ACNode **)calloc(automaton->id_count, sizeof(ACNode *));
    if (queue == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    int front = 0, rear = 0;
    uint32_t new_id = 0;
    queue[rear++] = automaton->root;

    while (front < rear) {
        ACNode *cur = queue[front++];
        cur->node_idx = new_id++;

        if (cur->output != NULL) {
            free(cur->output);
            cur->output = NULL;
        }
        cur->out_size = 0;

        for (int i = 0; i < BYTE_MAX_SIZE; i++) {
            if (cur->is_non_child_node[i]) {
                cur->children[i] = NULL;
                cur->is_non_child_node[i] = false;
            }
        }

        for (int i = 0; i < BYTE_MAX_SIZE; i++) {
            if (cur->children[i] != NULL && !cur->is_non_child_node[i]) {
                queue[rear++] = cur->children[i];
            }
        }
    }

    automaton->id_count = new_id;
    free(queue);
    return MMPM_SUCCESS;
}

static int rebuild_output_with_current_fail(ACAutomaton *automaton)
{
    ACNode **queue = (ACNode **)calloc(automaton->id_count, sizeof(ACNode *));
    if (queue == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    int front = 0, rear = 0;
    int ret = MMPM_SUCCESS;
    queue[rear++] = automaton->root;

    while (front < rear) {
        ACNode *cur = queue[front++];

        if (cur != automaton->root) {
            if (cur->parent == automaton->root) {
                if (cur->id_size > 0) {
                    ret = node_output_integrate_for_root_child(cur);
                    if (ret != MMPM_SUCCESS) {
                        break;
                    }
                }
            } else {
                ret = node_output_integrate(cur);
                if (ret != MMPM_SUCCESS) {
                    break;
                }
            }
        }

        for (int i = 0; i < BYTE_MAX_SIZE; i++) {
            if (cur->children[i] != NULL && !cur->is_non_child_node[i]) {
                queue[rear++] = cur->children[i];
            }
        }
    }

    free(queue);
    return ret;
}

static int fill_virtual_edges_by_current_fail(ACAutomaton *automaton)
{
    ACNode **queue = (ACNode **)calloc(automaton->id_count, sizeof(ACNode *));
    if (queue == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    ACNode *root = automaton->root;
    int front = 0, rear = 0;

    for (int i = 0; i < BYTE_MAX_SIZE; i++) {
        if (root->children[i] != NULL && !root->is_non_child_node[i]) {
            queue[rear++] = root->children[i];
        } else {
            root->children[i] = root;
            root->is_non_child_node[i] = true;
        }
    }

    while (front < rear) {
        ACNode *cur = queue[front++];

        for (int i = 0; i < BYTE_MAX_SIZE; i++) {
            if (cur->children[i] == NULL) {
                ACNode *tmp = cur->fail;
                while (tmp != root && (tmp->children[i] == NULL || tmp->is_non_child_node[i])) {
                    tmp = tmp->fail;
                }
                if (tmp->children[i] != NULL && !tmp->is_non_child_node[i]) {
                    cur->children[i] = tmp->children[i];
                } else {
                    cur->children[i] = root;
                }
                cur->is_non_child_node[i] = true;
            }
        }

        for (int i = 0; i < BYTE_MAX_SIZE; i++) {
            if (cur->children[i] != NULL && !cur->is_non_child_node[i]) {
                queue[rear++] = cur->children[i];
            }
        }
    }

    free(queue);
    return MMPM_SUCCESS;
}

static int rebuild_table_output_after_local_fail(ACAutomaton *automaton)
{
    free(automaton->table);
    automaton->table = NULL;
    if (automaton->output != NULL) {
        free(automaton->output);
        automaton->output = NULL;
    }

    int ret = clear_virtual_edges_and_reindex(automaton);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = rebuild_output_with_current_fail(automaton);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = fill_virtual_edges_by_current_fail(automaton);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    return ac_table_build(automaton);
}

int ac_automaton_remove(ACAutomaton *automaton, uint32_t alias, const uint8_t *pattern, uint32_t len)
{
    if (automaton == NULL || pattern == NULL || len == 0) {
        return MMPM_INVALID_PARA;
    }

    if (automaton->root == NULL) {
        return MMPM_INVALID_PARA;
    }

    ACNode *node = automaton->root;
    for (uint32_t i = 0; i < len; i++) {
        if (node->children[pattern[i]] == NULL || node->is_non_child_node[pattern[i]]) {
            return MMPM_FIND_FAIL;
        }
        node = node->children[pattern[i]];
    }

    if (node->id_size == 0) {
        return MMPM_FIND_FAIL;
    }

    int alias_idx = -1;
    for (int i = 0; i < node->id_size; i++) {
        if (node->id_set[i] == (int)alias) {
            alias_idx = i;
            break;
        }
    }
    if (alias_idx == -1) {
        return MMPM_FIND_FAIL;
    }

    memmove(&node->id_set[alias_idx], &node->id_set[alias_idx + 1],
        (size_t)(node->id_size - alias_idx - 1) * sizeof(int));
    node->id_size--;

    DeletedNodeInfo *deleted_infos = (DeletedNodeInfo *)calloc(automaton->id_count, sizeof(DeletedNodeInfo));
    if (deleted_infos == NULL) {
        return MMPM_MALLOC_FAIL;
    }
    uint32_t deleted_cnt = 0;

    prune_empty_nodes_collect(automaton, node, deleted_infos, &deleted_cnt);

    int ret = update_fail_pointers_local(automaton, deleted_infos, deleted_cnt);
    free(deleted_infos);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = rebuild_table_output_after_local_fail(automaton);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    if (automaton->pattern_count > 0) {
        automaton->pattern_count--;
    }
    return MMPM_SUCCESS;
}

int ac_automaton_match(ACAutomaton *automaton, const uint8_t *data, uint32_t len)
{
    uint8_t idx;
    int table_ret = MMPM_FIND_FAIL;

    if (automaton == NULL || automaton->table == NULL || automaton->output == NULL || data == NULL) {
        return MMPM_INVALID_PARA;
    }

    uint32_t node_index = 0;
    for (uint32_t i = 0; i < len; i++) {
        idx = data[i];
        node_index = automaton->table[(uint64_t)node_index * BYTE_MAX_SIZE + idx];
        if (automaton->output[node_index] != NULL)
            table_ret = MMPM_SUCCESS;
    }
    return table_ret;
}

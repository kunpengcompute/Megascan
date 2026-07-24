/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Stateful handler, logical and allocator session commands.
 */
#include "api_session_extended_demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo_common.h"
#include "ms.h"
#include "ms_allocator.h"
#include "ms_compile.h"
#include "ms_expr.h"
#include "ms_handler.h"
#include "ms_logical.h"
#include "ms_rule.h"
#include "ms_runtime.h"

#define EXT_MAX_HANDLES 64
#define EXT_NAME_SIZE 32
#define EXT_MAX_CAPACITY 65536u
#define EXT_MAX_LOGICAL_COUNT 1048576u

#define ALLOCATOR_DATABASE 1u
#define ALLOCATOR_SCRATCH 2u
#define ALLOCATOR_STREAM 4u
#define ALLOCATOR_MISC 8u
#define ALLOCATOR_ALL 15u

typedef struct {
    char name[EXT_NAME_SIZE];
    ms_handler_t value;
} ext_handler_t;

typedef struct {
    char name[EXT_NAME_SIZE];
    ms_stream_t *value;
    int handler_slot;
} ext_stream_t;

typedef struct {
    char name[EXT_NAME_SIZE];
    ms_logical_tree_t value;
} ext_tree_t;

typedef struct {
    char name[EXT_NAME_SIZE];
    uint8_t *value;
    uint32_t count;
} ext_vector_t;

typedef struct {
    ext_handler_t handlers[EXT_MAX_HANDLES];
    ext_stream_t streams[EXT_MAX_HANDLES];
    ext_tree_t trees[EXT_MAX_HANDLES];
    ext_vector_t vectors[EXT_MAX_HANDLES];
    uint32_t allocator_mask;
    size_t allocs;
    size_t callocs;
    size_t frees;
} ext_state_t;

static ext_state_t g_ext;

static int ext_valid_name(const char *name)
{
    size_t i;
    size_t length = name == NULL ? 0 : strlen(name);

    if (length == 0 || length >= EXT_NAME_SIZE) return 0;
    for (i = 0; i < length; i++) {
        char ch = name[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) {
            return 0;
        }
    }
    return 1;
}

static void ext_store_name(char destination[EXT_NAME_SIZE], const char *name)
{
    (void)snprintf(destination, EXT_NAME_SIZE, "%s", name);
}

static int find_handler(const char *name)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.handlers[i].name[0] != '\0' &&
            demo_same(g_ext.handlers[i].name, name)) return i;
    }
    return -1;
}

static int find_stream(const char *name)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.streams[i].name[0] != '\0' &&
            demo_same(g_ext.streams[i].name, name)) return i;
    }
    return -1;
}

static int find_tree(const char *name)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.trees[i].name[0] != '\0' &&
            demo_same(g_ext.trees[i].name, name)) return i;
    }
    return -1;
}

static int find_vector(const char *name)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.vectors[i].name[0] != '\0' &&
            demo_same(g_ext.vectors[i].name, name)) return i;
    }
    return -1;
}

static int free_handler_slot(void)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.handlers[i].name[0] == '\0') return i;
    }
    return -1;
}

static int free_stream_slot(void)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.streams[i].name[0] == '\0') return i;
    }
    return -1;
}

static int free_tree_slot(void)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.trees[i].name[0] == '\0') return i;
    }
    return -1;
}

static int free_vector_slot(void)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.vectors[i].name[0] == '\0') return i;
    }
    return -1;
}

int demo_extended_session_name_in_use(const char *name)
{
    return find_handler(name) >= 0 || find_stream(name) >= 0 ||
           find_tree(name) >= 0 || find_vector(name) >= 0;
}

static int parse_capacity(const char *text, uint32_t *capacity)
{
    if (demo_parse_u32(text, capacity) != 0 ||
        *capacity > EXT_MAX_CAPACITY) {
        printf("ERROR stage=CLI field=capacity reason=OUT_OF_RANGE "
               "value=%s max=%u\n", text, EXT_MAX_CAPACITY);
        return -1;
    }
    return 0;
}

static uint32_t *alloc_ids(uint32_t capacity)
{
    return capacity == 0 ? NULL :
           (uint32_t *)calloc(capacity, sizeof(uint32_t));
}

static void print_ids(const uint32_t *ids, uint32_t count, uint32_t capacity)
{
    uint32_t i;
    uint32_t printable = count < capacity ? count : capacity;

    printf(" count=%u ids=", count);
    for (i = 0; i < printable; i++) {
        printf("%s%u", i == 0 ? "" : ",", ids[i]);
    }
    if (count > capacity) printf(" required_capacity=%u", count);
    printf("\n");
}

static int parse_rules(int argc, char **argv, ms_rule_t **rules,
                       uint32_t *count)
{
    uint32_t i;

    if (argc <= 0 || argc % 3 != 0) return -1;
    *count = (uint32_t)argc / 3u;
    *rules = (ms_rule_t *)calloc(*count, sizeof(**rules));
    if (*rules == NULL) return -1;
    for (i = 0; i < *count; i++) {
        const char *key = argv[i * 3u + 2u];
        if (demo_parse_u32(argv[i * 3u], &(*rules)[i].alias) != 0 ||
            demo_parse_u32(argv[i * 3u + 1u], &(*rules)[i].key_len) != 0 ||
            (!demo_same(key, "-") &&
             (*rules)[i].key_len > strlen(key))) {
            free(*rules);
            *rules = NULL;
            *count = 0;
            return -1;
        }
        (*rules)[i].key = demo_same(key, "-") ? NULL : (uint8_t *)key;
    }
    return 0;
}

static int parse_aliases(const char *text, uint32_t **aliases,
                         uint32_t *count)
{
    const char *cursor = text;
    uint32_t capacity = 4;

    *aliases = (uint32_t *)calloc(capacity, sizeof(**aliases));
    if (*aliases == NULL || text == NULL || text[0] == '\0') {
        free(*aliases);
        *aliases = NULL;
        return -1;
    }
    while (*cursor != '\0') {
        char *end = NULL;
        unsigned long parsed = strtoul(cursor, &end, 10);
        uint32_t *grown;

        if (end == cursor || parsed > UINT32_MAX ||
            (*end != ',' && *end != '\0')) {
            free(*aliases);
            *aliases = NULL;
            return -1;
        }
        if (*count == capacity) {
            capacity *= 2u;
            grown = (uint32_t *)realloc(*aliases,
                                        capacity * sizeof(**aliases));
            if (grown == NULL) {
                free(*aliases);
                *aliases = NULL;
                return -1;
            }
            *aliases = grown;
        }
        (*aliases)[(*count)++] = (uint32_t)parsed;
        cursor = *end == ',' ? end + 1 : end;
    }
    return *count == 0 ? -1 : 0;
}

static uint32_t ext_hash(const uint8_t *key, uint32_t key_len)
{
    uint32_t hash = 2166136261u;
    uint32_t i;
    for (i = 0; i < key_len; i++) {
        hash ^= key[i];
        hash *= 16777619u;
    }
    return hash;
}

static int command_handler_init(int argc, char **argv)
{
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    int slot;
    int ret;

    if (argc != 5 || !ext_valid_name(argv[1]) ||
        demo_parse_matcher(argv[2], &type) != 0 ||
        demo_parse_u32(argv[3], &buckets) != 0 ||
        demo_parse_u32(argv[4], &secondary) != 0) {
        printf("ERROR stage=CLI usage=ms_handler_init "
               "<handler> <hash|ac> <buckets> <secondary>\n");
        return 1;
    }
    if (demo_extended_session_name_in_use(argv[1])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[1]);
        return 1;
    }
    slot = free_handler_slot();
    if (slot < 0) {
        printf("ERROR stage=STATE type=handler reason=HANDLE_TABLE_FULL\n");
        return 1;
    }
    ret = ms_handler_init(&g_ext.handlers[slot].value, type,
                          buckets, secondary);
    if (ret == MS_SUCCESS) ext_store_name(g_ext.handlers[slot].name, argv[1]);
    printf("%s api=ms_handler_init ret=%d(%s) handler=%s state=%s\n",
           ret == MS_SUCCESS ? "OK" : "ERROR", ret, demo_ret_name(ret),
           argv[1], ret == MS_SUCCESS ? "CREATED" : "NOT_CREATED");
    return demo_result_from_ret(ret);
}

static int handler_has_stream(int handler_slot)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.streams[i].name[0] != '\0' &&
            g_ext.streams[i].handler_slot == handler_slot) return 1;
    }
    return 0;
}

static int command_handler_destroy(int argc, char **argv)
{
    int slot;
    int ret;

    if (argc != 2 || (slot = find_handler(argv[1])) < 0) {
        printf("ERROR stage=STATE api=ms_handler_destroy handle=%s "
               "reason=NOT_FOUND\n", argc > 1 ? argv[1] : "-");
        return 1;
    }
    if (handler_has_stream(slot)) {
        printf("ERROR stage=STATE api=ms_handler_destroy handler=%s "
               "reason=HAS_CHILD_STREAM\n", argv[1]);
        return 1;
    }
    ret = ms_handler_destroy(&g_ext.handlers[slot].value);
    printf("%s api=ms_handler_destroy ret=%d(%s) handler=%s state=%s\n",
           ret == MS_SUCCESS ? "OK" : "ERROR", ret, demo_ret_name(ret),
           argv[1], ret == MS_SUCCESS ? "RELEASED" : "NOT_RELEASED");
    if (ret == MS_SUCCESS) {
        (void)memset(&g_ext.handlers[slot], 0, sizeof(g_ext.handlers[slot]));
    }
    return demo_result_from_ret(ret);
}

static int command_set_hash(int argc, char **argv)
{
    int slot;
    int ret;

    if (argc != 3 || (slot = find_handler(argv[1])) < 0 ||
        (!demo_same(argv[2], "custom") && !demo_same(argv[2], "null"))) {
        printf("ERROR stage=CLI usage=ms_set_hash_func "
               "<handler> <custom|null>\n");
        return 1;
    }
    ret = ms_set_hash_func(&g_ext.handlers[slot].value,
                           demo_same(argv[2], "custom") ? ext_hash : NULL);
    printf("%s api=ms_set_hash_func ret=%d(%s) handler=%s mode=%s\n",
           ret == MS_SUCCESS ? "OK" : "ERROR", ret, demo_ret_name(ret),
           argv[1], argv[2]);
    return demo_result_from_ret(ret);
}

static int command_handler_rules(const char *api, int argc, char **argv)
{
    ms_rule_t *rules = NULL;
    uint32_t count = 0;
    uint32_t rule_num = 0;
    int slot;
    int ret;

    if (argc < 6 || (slot = find_handler(argv[1])) < 0 ||
        demo_parse_u32(argv[argc - 1], &rule_num) != 0 ||
        parse_rules(argc - 3, argv + 2, &rules, &count) != 0 ||
        count != rule_num) {
        printf("ERROR stage=CLI usage=%s <handler> "
               "<alias key_len key|->... <rule_num>\n", api);
        free(rules);
        return 1;
    }
    ret = demo_same(api, "ms_add_rule") ?
          ms_add_rule(&g_ext.handlers[slot].value, rules, rule_num) :
          ms_remove_rule(&g_ext.handlers[slot].value, rules, rule_num);
    printf("%s api=%s ret=%d(%s) handler=%s rule_num=%u\n",
           ret == MS_SUCCESS ? "OK" : "ERROR", api, ret,
           demo_ret_name(ret), argv[1], rule_num);
    free(rules);
    return demo_result_from_ret(ret);
}

static int command_find_rule(int argc, char **argv)
{
    ms_rule_t rule = {0};
    int slot;
    int ret;

    if (argc != 5 || (slot = find_handler(argv[1])) < 0 ||
        demo_parse_u32(argv[2], &rule.alias) != 0 ||
        demo_parse_u32(argv[3], &rule.key_len) != 0 ||
        (!demo_same(argv[4], "-") && rule.key_len > strlen(argv[4]))) {
        printf("ERROR stage=CLI usage=ms_find_rule "
               "<handler> <alias> <key_len> <key|->\n");
        return 1;
    }
    rule.key = demo_same(argv[4], "-") ? NULL : (uint8_t *)argv[4];
    ret = ms_find_rule(&g_ext.handlers[slot].value, &rule);
    printf("%s api=ms_find_rule ret=%d(%s) handler=%s alias=%u\n",
           ret == MS_SUCCESS ? "OK" :
           ret == MS_FIND_FAIL ? "NO_MATCH" : "ERROR",
           ret, demo_ret_name(ret), argv[1], rule.alias);
    return demo_result_from_ret(ret);
}

static int command_compile_handler(const char *api, int argc, char **argv)
{
    demo_expr_spec_t spec = {0};
    int extended = demo_same(api, "ms_compile_multi_ext");
    int add_exprs = demo_same(api, "ms_add_exprs");
    int slot;
    int ret;

    if (argc < 6 || (slot = find_handler(argv[1])) < 0 ||
        demo_parse_expr_spec(argc - 2, argv + 2, extended,
                             add_exprs, &spec) != 0) {
        printf("ERROR stage=CLI usage=%s <handler> <arrays...> <elements>\n",
               api);
        return 1;
    }
    ret = extended ?
          ms_compile_multi_ext(&g_ext.handlers[slot].value,
                               spec.expressions, spec.flags, spec.ids,
                               spec.ext, spec.count) :
          add_exprs ?
          ms_add_exprs(&g_ext.handlers[slot].value,
                       spec.expressions, spec.ids, spec.flags, spec.count) :
          ms_compile_multi(&g_ext.handlers[slot].value,
                           spec.expressions, spec.flags, spec.ids, spec.count);
    printf("%s api=%s ret=%d(%s) handler=%s elements=%u\n",
           ret == MS_SUCCESS ? "OK" : "ERROR", api, ret,
           demo_ret_name(ret), argv[1], spec.count);
    demo_free_expr_spec(&spec);
    return demo_result_from_ret(ret);
}

static int command_handler_scan(const char *api, int argc, char **argv)
{
    uint32_t len;
    uint32_t capacity;
    uint32_t count = 0;
    uint32_t *ids;
    int slot;
    int ret;

    if (argc != 5 || (slot = find_handler(argv[1])) < 0 ||
        demo_parse_u32(argv[3], &len) != 0 ||
        len > strlen(argv[2]) || parse_capacity(argv[4], &capacity) != 0) {
        printf("ERROR stage=CLI usage=%s "
               "<handler> <data> <len> <capacity>\n", api);
        return 1;
    }
    ids = alloc_ids(capacity);
    if (capacity != 0 && ids == NULL) return 1;
    ret = demo_same(api, "ms_scan_expr") ?
          ms_scan_expr(&g_ext.handlers[slot].value,
                       (const uint8_t *)argv[2], len,
                       ids, &count, capacity) :
          ms_compile_scan_ac_combinations(&g_ext.handlers[slot].value,
                                          (const uint8_t *)argv[2], len,
                                          ids, &count, capacity);
    printf("%s api=%s ret=%d(%s) handler=%s",
           ret == MS_SUCCESS ? "OK" :
           ret == MS_FIND_FAIL ? "NO_MATCH" : "ERROR",
           api, ret, demo_ret_name(ret), argv[1]);
    print_ids(ids, count, capacity);
    free(ids);
    return demo_result_from_ret(ret);
}

static int command_eval_combinations(int argc, char **argv)
{
    uint32_t *aliases = NULL;
    uint32_t alias_count = 0;
    uint32_t supplied_count;
    uint32_t capacity;
    uint32_t *ids = NULL;
    uint32_t count = 0;
    int slot;
    int ret;

    if (argc != 5 || (slot = find_handler(argv[1])) < 0 ||
        demo_parse_u32(argv[3], &supplied_count) != 0 ||
        parse_capacity(argv[4], &capacity) != 0 ||
        parse_aliases(argv[2], &aliases, &alias_count) != 0 ||
        supplied_count != alias_count) {
        printf("ERROR stage=CLI usage=ms_compile_eval_combinations "
               "<handler> <alias_csv> <alias_count> <capacity>\n");
        free(aliases);
        return 1;
    }
    ids = alloc_ids(capacity);
    if (capacity != 0 && ids == NULL) {
        free(aliases);
        return 1;
    }
    ret = ms_compile_eval_combinations(&g_ext.handlers[slot].value,
                                       aliases, alias_count,
                                       ids, &count, capacity);
    printf("%s api=ms_compile_eval_combinations ret=%d(%s) handler=%s",
           ret == MS_SUCCESS ? "OK" :
           ret == MS_FIND_FAIL ? "NO_MATCH" : "ERROR",
           ret, demo_ret_name(ret), argv[1]);
    print_ids(ids, count, capacity);
    free(ids);
    free(aliases);
    return demo_result_from_ret(ret);
}

static int command_open_handler_stream(int argc, char **argv)
{
    int handler_slot;
    int stream_slot;
    int ret;

    if (argc != 3 || !ext_valid_name(argv[2]) ||
        (handler_slot = find_handler(argv[1])) < 0) {
        printf("ERROR stage=CLI usage=ms_open_handler_stream "
               "<handler> <stream>\n");
        return 1;
    }
    if (demo_extended_session_name_in_use(argv[2])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[2]);
        return 1;
    }
    stream_slot = free_stream_slot();
    if (stream_slot < 0) return 1;
    ret = ms_open_handler_stream(&g_ext.handlers[handler_slot].value,
                                 &g_ext.streams[stream_slot].value);
    if (ret == MS_SUCCESS) {
        ext_store_name(g_ext.streams[stream_slot].name, argv[2]);
        g_ext.streams[stream_slot].handler_slot = handler_slot;
    }
    printf("%s api=ms_open_handler_stream ret=%d(%s) handler=%s "
           "stream=%s state=%s\n", ret == MS_SUCCESS ? "OK" : "ERROR",
           ret, demo_ret_name(ret), argv[1], argv[2],
           ret == MS_SUCCESS ? "CREATED" : "NOT_CREATED");
    return demo_result_from_ret(ret);
}

static int command_handler_stream_data(const char *api, int argc, char **argv)
{
    uint32_t len;
    uint32_t capacity = 0;
    uint32_t count = 0;
    uint32_t *ids = NULL;
    int slot;
    int ret;

    if ((demo_same(api, "ms_scan_rule_stream") && argc != 4) ||
        (!demo_same(api, "ms_scan_rule_stream") && argc != 5) ||
        (slot = find_stream(argv[1])) < 0 ||
        demo_parse_u32(argv[3], &len) != 0 ||
        len > strlen(argv[2]) ||
        (!demo_same(api, "ms_scan_rule_stream") &&
         parse_capacity(argv[4], &capacity) != 0)) {
        printf("ERROR stage=CLI usage=%s <stream> <data> <len>%s\n",
               api, demo_same(api, "ms_scan_rule_stream") ?
               "" : " <capacity>");
        return 1;
    }
    if (!demo_same(api, "ms_scan_rule_stream")) {
        ids = alloc_ids(capacity);
        if (capacity != 0 && ids == NULL) return 1;
        ret = ms_scan_expr_stream(g_ext.streams[slot].value,
                                  (const uint8_t *)argv[2], len,
                                  ids, &count, capacity);
    } else {
        ret = ms_scan_rule_stream(g_ext.streams[slot].value,
                                  (const uint8_t *)argv[2], len);
    }
    printf("%s api=%s ret=%d(%s) stream=%s",
           ret == MS_SUCCESS ? "OK" :
           ret == MS_FIND_FAIL ? "NO_MATCH" : "ERROR",
           api, ret, demo_ret_name(ret), argv[1]);
    if (!demo_same(api, "ms_scan_rule_stream")) {
        print_ids(ids, count, capacity);
    } else {
        printf("\n");
    }
    free(ids);
    return demo_result_from_ret(ret);
}

static int command_handler_stream_simple(const char *api, int argc,
                                         char **argv)
{
    uint32_t capacity = 0;
    uint32_t count = 0;
    uint32_t *ids = NULL;
    int slot;
    int ret;

    if ((demo_same(api, "ms_scan_expr_stream_flush") && argc != 3) ||
        (!demo_same(api, "ms_scan_expr_stream_flush") && argc != 2) ||
        (slot = find_stream(argv[1])) < 0 ||
        (demo_same(api, "ms_scan_expr_stream_flush") &&
         parse_capacity(argv[2], &capacity) != 0)) {
        printf("ERROR stage=CLI usage=%s <stream>%s\n", api,
               demo_same(api, "ms_scan_expr_stream_flush") ?
               " <capacity>" : "");
        return 1;
    }
    if (demo_same(api, "ms_scan_expr_stream_flush")) {
        ids = alloc_ids(capacity);
        if (capacity != 0 && ids == NULL) return 1;
        ret = ms_scan_expr_stream_flush(g_ext.streams[slot].value,
                                        ids, &count, capacity);
    } else if (demo_same(api, "ms_reset_stream")) {
        ret = ms_reset_stream(g_ext.streams[slot].value);
    } else {
        ret = ms_close_handler_stream(g_ext.streams[slot].value);
        if (ret == MS_SUCCESS) {
            (void)memset(&g_ext.streams[slot], 0,
                         sizeof(g_ext.streams[slot]));
        }
    }
    printf("%s api=%s ret=%d(%s) stream=%s",
           ret == MS_SUCCESS ? "OK" :
           ret == MS_FIND_FAIL ? "NO_MATCH" : "ERROR",
           api, ret, demo_ret_name(ret), argv[1]);
    if (demo_same(api, "ms_scan_expr_stream_flush")) {
        print_ids(ids, count, capacity);
    } else {
        printf(" state=%s\n",
               demo_same(api, "ms_close_handler_stream") &&
               ret == MS_SUCCESS ? "RELEASED" : "ACTIVE");
    }
    free(ids);
    return demo_result_from_ret(ret);
}

static int command_logical_parse(int argc, char **argv)
{
    int slot;
    int ret;

    if (argc != 3 || !ext_valid_name(argv[1])) {
        printf("ERROR stage=CLI usage=ms_logical_parse <tree> <expression>\n");
        return 1;
    }
    if (demo_extended_session_name_in_use(argv[1])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[1]);
        return 1;
    }
    slot = free_tree_slot();
    if (slot < 0) return 1;
    ret = ms_logical_parse(argv[2], &g_ext.trees[slot].value);
    if (ret == MS_SUCCESS) ext_store_name(g_ext.trees[slot].name, argv[1]);
    printf("%s api=ms_logical_parse ret=%d(%s) tree=%s state=%s\n",
           ret == MS_SUCCESS ? "OK" : "ERROR", ret, demo_ret_name(ret),
           argv[1], ret == MS_SUCCESS ? "CREATED" : "NOT_CREATED");
    return demo_result_from_ret(ret);
}

static int command_tree_create_op(int argc, char **argv)
{
    ms_logical_op_t *op;
    uint32_t operation;
    int slot;

    if (argc != 6 || !ext_valid_name(argv[1])) {
        printf("ERROR stage=CLI usage=logical_tree_create_op "
               "<tree> <result_id> <not|and|or> <left> <right>\n");
        return 1;
    }
    if (demo_extended_session_name_in_use(argv[1])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[1]);
        return 1;
    }
    slot = free_tree_slot();
    if (slot < 0) return 1;
    op = (ms_logical_op_t *)calloc(1, sizeof(*op));
    if (op == NULL || demo_parse_u32(argv[2], &op->id) != 0 ||
        demo_parse_u32(argv[4], &op->left) != 0 ||
        demo_parse_u32(argv[5], &op->right) != 0) {
        free(op);
        printf("ERROR stage=CLI usage=logical_tree_create_op "
               "<tree> <result_id> <not|and|or> <left> <right>\n");
        return 1;
    }
    operation = demo_same(argv[3], "not") ? MS_LOGICAL_OP_NOT :
                demo_same(argv[3], "and") ? MS_LOGICAL_OP_AND :
                demo_same(argv[3], "or") ? MS_LOGICAL_OP_OR : UINT32_MAX;
    if (operation == UINT32_MAX) {
        free(op);
        printf("ERROR stage=CLI field=operation value=%s\n", argv[3]);
        return 1;
    }
    op->op = operation;
    g_ext.trees[slot].value.ops = op;
    g_ext.trees[slot].value.op_count = 1;
    g_ext.trees[slot].value.op_capacity = 1;
    g_ext.trees[slot].value.result = op->id;
    ext_store_name(g_ext.trees[slot].name, argv[1]);
    printf("OK command=logical_tree_create_op tree=%s result_id=%u "
           "state=CREATED\n", argv[1], op->id);
    return 0;
}

static int command_tree_destroy(int argc, char **argv)
{
    int slot;

    if (argc != 2 || (slot = find_tree(argv[1])) < 0) {
        printf("ERROR stage=STATE api=ms_logical_tree_destroy handle=%s "
               "reason=NOT_FOUND\n", argc > 1 ? argv[1] : "-");
        return 1;
    }
    ms_logical_tree_destroy(&g_ext.trees[slot].value);
    (void)memset(&g_ext.trees[slot], 0, sizeof(g_ext.trees[slot]));
    printf("OK api=ms_logical_tree_destroy ret=0(%s) tree=%s "
           "state=RELEASED\n", demo_ret_name(MS_SUCCESS), argv[1]);
    return 0;
}

static int command_vector_create(int argc, char **argv)
{
    uint32_t count;
    size_t bytes;
    int slot;

    if (argc != 3 || !ext_valid_name(argv[1]) ||
        demo_parse_u32(argv[2], &count) != 0 ||
        count == 0 || count > EXT_MAX_LOGICAL_COUNT) {
        printf("ERROR stage=CLI usage=logical_vector_create "
               "<vector> <logical_count>\n");
        return 1;
    }
    if (demo_extended_session_name_in_use(argv[1])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[1]);
        return 1;
    }
    slot = free_vector_slot();
    if (slot < 0) return 1;
    bytes = (count + 7u) / 8u;
    g_ext.vectors[slot].value = (uint8_t *)calloc(bytes, 1);
    if (g_ext.vectors[slot].value == NULL) return 1;
    g_ext.vectors[slot].count = count;
    ext_store_name(g_ext.vectors[slot].name, argv[1]);
    printf("OK command=logical_vector_create vector=%s logical_count=%u "
           "state=CREATED\n", argv[1], count);
    return 0;
}

static int command_vector_free(int argc, char **argv)
{
    int slot;
    if (argc != 2 || (slot = find_vector(argv[1])) < 0) {
        printf("ERROR stage=STATE command=logical_vector_free handle=%s "
               "reason=NOT_FOUND\n", argc > 1 ? argv[1] : "-");
        return 1;
    }
    free(g_ext.vectors[slot].value);
    (void)memset(&g_ext.vectors[slot], 0, sizeof(g_ext.vectors[slot]));
    printf("OK command=logical_vector_free vector=%s state=RELEASED\n",
           argv[1]);
    return 0;
}

static int command_logical_vector_api(const char *api, int argc, char **argv)
{
    uint32_t key = 0;
    uint32_t raw_value = 0;
    uint8_t value = 0;
    int slot;
    int ret;

    if ((slot = argc > 1 ? find_vector(argv[1]) : -1) < 0) {
        printf("ERROR stage=STATE api=%s vector=%s reason=NOT_FOUND\n",
               api, argc > 1 ? argv[1] : "-");
        return 1;
    }
    if (demo_same(api, "ms_logical_clear")) {
        if (argc != 2) return 1;
        ret = ms_logical_clear(g_ext.vectors[slot].value,
                               g_ext.vectors[slot].count);
    } else {
        if (argc != (demo_same(api, "ms_logical_set") ? 4 : 3) ||
            demo_parse_u32(argv[2], &key) != 0 ||
            (demo_same(api, "ms_logical_set") &&
             (demo_parse_u32(argv[3], &raw_value) != 0 ||
              raw_value > 1u))) {
            printf("ERROR stage=CLI usage=%s <vector> <key>%s\n",
                   api, demo_same(api, "ms_logical_set") ? " <0|1>" : "");
            return 1;
        }
        ret = demo_same(api, "ms_logical_set") ?
              ms_logical_set(g_ext.vectors[slot].value,
                             g_ext.vectors[slot].count, key,
                             (uint8_t)raw_value) :
              ms_logical_get(g_ext.vectors[slot].value,
                             g_ext.vectors[slot].count, key, &value);
    }
    printf("%s api=%s ret=%d(%s) vector=%s",
           ret == MS_SUCCESS ? "OK" : "ERROR", api, ret,
           demo_ret_name(ret), argv[1]);
    if (demo_same(api, "ms_logical_get")) printf(" value=%u", value);
    printf("\n");
    return demo_result_from_ret(ret);
}

static int command_logical_eval(const char *api, int argc, char **argv)
{
    uint8_t matched = 0;
    int tree_slot;
    int vector_slot;
    int ret;

    if (argc != 3 || (tree_slot = find_tree(argv[1])) < 0 ||
        (vector_slot = find_vector(argv[2])) < 0) {
        printf("ERROR stage=STATE usage=%s <tree> <vector> "
               "reason=NOT_FOUND\n", api);
        return 1;
    }
    ret = demo_same(api, "ms_logical_eval") ?
          ms_logical_eval(&g_ext.trees[tree_slot].value,
                          g_ext.vectors[vector_slot].value,
                          g_ext.vectors[vector_slot].count, &matched) :
          ms_logical_eval_inplace(&g_ext.trees[tree_slot].value,
                                  g_ext.vectors[vector_slot].value,
                                  g_ext.vectors[vector_slot].count, &matched);
    printf("%s api=%s ret=%d(%s) tree=%s vector=%s matched=%u\n",
           ret == MS_SUCCESS ? "OK" : "ERROR", api, ret,
           demo_ret_name(ret), argv[1], argv[2], matched);
    return demo_result_from_ret(ret);
}

static void *session_alloc(size_t size)
{
    g_ext.allocs++;
    return malloc(size);
}

static void *session_calloc(size_t nmemb, size_t size)
{
    g_ext.callocs++;
    return calloc(nmemb, size);
}

static void session_free(void *ptr)
{
    g_ext.frees++;
    free(ptr);
}

static int command_allocator(const char *api, int argc, char **argv)
{
    int custom_alloc;
    int custom_calloc;
    int custom_free;
    int any_custom;
    uint32_t bit;
    int ret;

    if (argc != 4 ||
        (!demo_same(argv[1], "custom") &&
         !demo_same(argv[1], "default")) ||
        (!demo_same(argv[2], "custom") &&
         !demo_same(argv[2], "default")) ||
        (!demo_same(argv[3], "custom") &&
         !demo_same(argv[3], "default"))) {
        printf("ERROR stage=CLI usage=%s "
               "<alloc:custom|default> <calloc:custom|default> "
               "<free:custom|default>\n", api);
        return 1;
    }
    custom_alloc = demo_same(argv[1], "custom");
    custom_calloc = demo_same(argv[2], "custom");
    custom_free = demo_same(argv[3], "custom");
    any_custom = custom_alloc || custom_calloc || custom_free;
    if (demo_same(api, "ms_set_allocator")) {
        bit = ALLOCATOR_ALL;
        ret = ms_set_allocator(custom_alloc ? session_alloc : NULL,
                               custom_calloc ? session_calloc : NULL,
                               custom_free ? session_free : NULL);
    } else if (demo_same(api, "ms_set_database_allocator")) {
        bit = ALLOCATOR_DATABASE;
        ret = ms_set_database_allocator(custom_alloc ? session_alloc : NULL,
                                        custom_calloc ? session_calloc : NULL,
                                        custom_free ? session_free : NULL);
    } else if (demo_same(api, "ms_set_scratch_allocator")) {
        bit = ALLOCATOR_SCRATCH;
        ret = ms_set_scratch_allocator(custom_alloc ? session_alloc : NULL,
                                       custom_calloc ? session_calloc : NULL,
                                       custom_free ? session_free : NULL);
    } else if (demo_same(api, "ms_set_stream_allocator")) {
        bit = ALLOCATOR_STREAM;
        ret = ms_set_stream_allocator(custom_alloc ? session_alloc : NULL,
                                      custom_calloc ? session_calloc : NULL,
                                      custom_free ? session_free : NULL);
    } else {
        bit = ALLOCATOR_MISC;
        ret = ms_set_misc_allocator(custom_alloc ? session_alloc : NULL,
                                    custom_calloc ? session_calloc : NULL,
                                    custom_free ? session_free : NULL);
    }
    if (ret == MS_SUCCESS) {
        if (any_custom) g_ext.allocator_mask |= bit;
        else g_ext.allocator_mask &= ~bit;
    }
    printf("%s api=%s ret=%d(%s) mode=%s allocator_mask=%u\n",
           ret == MS_SUCCESS ? "OK" : "ERROR", api, ret,
           demo_ret_name(ret), any_custom ? "CUSTOM" : "DEFAULT",
           g_ext.allocator_mask);
    return demo_result_from_ret(ret);
}

static int command_allocator_stats(int argc, char **argv)
{
    if (argc > 2 || (argc == 2 && !demo_same(argv[1], "reset"))) {
        printf("ERROR stage=CLI usage=allocator_stats [reset]\n");
        return 1;
    }
    printf("ALLOCATOR_STATS malloc=%zu calloc=%zu free=%zu active_mask=%u\n",
           g_ext.allocs, g_ext.callocs, g_ext.frees,
           g_ext.allocator_mask);
    if (argc == 2) {
        g_ext.allocs = g_ext.callocs = g_ext.frees = 0;
        printf("ALLOCATOR_STATS state=RESET\n");
    }
    return 0;
}

int demo_extended_session_dispatch(int argc, char **argv)
{
    const char *api = argv[0];

    if (demo_same(api, "ms_handler_init")) {
        return command_handler_init(argc, argv);
    }
    if (demo_same(api, "ms_handler_destroy")) {
        return command_handler_destroy(argc, argv);
    }
    if (demo_same(api, "ms_set_hash_func")) {
        return command_set_hash(argc, argv);
    }
    if (demo_same(api, "ms_add_rule") || demo_same(api, "ms_remove_rule")) {
        return command_handler_rules(api, argc, argv);
    }
    if (demo_same(api, "ms_find_rule")) return command_find_rule(argc, argv);
    if (demo_same(api, "ms_compile_multi") ||
        demo_same(api, "ms_compile_multi_ext") ||
        demo_same(api, "ms_add_exprs")) {
        return command_compile_handler(api, argc, argv);
    }
    if (demo_same(api, "ms_scan_expr") ||
        demo_same(api, "ms_compile_scan_ac_combinations")) {
        return command_handler_scan(api, argc, argv);
    }
    if (demo_same(api, "ms_compile_eval_combinations")) {
        return command_eval_combinations(argc, argv);
    }
    if (demo_same(api, "ms_open_handler_stream")) {
        return command_open_handler_stream(argc, argv);
    }
    if (demo_same(api, "ms_scan_expr_stream") ||
        demo_same(api, "ms_scan_rule_stream")) {
        return command_handler_stream_data(api, argc, argv);
    }
    if (demo_same(api, "ms_scan_expr_stream_flush") ||
        demo_same(api, "ms_close_handler_stream")) {
        return command_handler_stream_simple(api, argc, argv);
    }
    if (demo_same(api, "ms_reset_stream") && argc == 2 &&
        find_stream(argv[1]) >= 0) {
        return command_handler_stream_simple(api, argc, argv);
    }
    if (demo_same(api, "ms_logical_parse")) {
        return command_logical_parse(argc, argv);
    }
    if (demo_same(api, "logical_tree_create_op")) {
        return command_tree_create_op(argc, argv);
    }
    if (demo_same(api, "ms_logical_tree_destroy")) {
        return command_tree_destroy(argc, argv);
    }
    if (demo_same(api, "logical_vector_create")) {
        return command_vector_create(argc, argv);
    }
    if (demo_same(api, "logical_vector_free")) {
        return command_vector_free(argc, argv);
    }
    if (demo_same(api, "ms_logical_set") ||
        demo_same(api, "ms_logical_get") ||
        demo_same(api, "ms_logical_clear")) {
        return command_logical_vector_api(api, argc, argv);
    }
    if (demo_same(api, "ms_logical_eval") ||
        demo_same(api, "ms_logical_eval_inplace")) {
        return command_logical_eval(api, argc, argv);
    }
    if ((strncmp(api, "ms_set_", 7) == 0 &&
         strstr(api, "allocator") != NULL)) {
        return command_allocator(api, argc, argv);
    }
    if (demo_same(api, "allocator_stats")) {
        return command_allocator_stats(argc, argv);
    }
    return -1;
}

void demo_extended_session_list_handles(void)
{
    int i;
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.handlers[i].name[0] != '\0') {
            printf("HANDLE type=handler name=%s matcher=%s built=%u\n",
                   g_ext.handlers[i].name,
                   g_ext.handlers[i].value.type == MS_MATCHER_TYPE_AC ?
                   "ac" : "hash",
                   g_ext.handlers[i].value.is_built ? 1u : 0u);
        }
    }
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.streams[i].name[0] != '\0') {
            printf("HANDLE type=handler_stream name=%s owner=%s\n",
                   g_ext.streams[i].name,
                   g_ext.handlers[g_ext.streams[i].handler_slot].name);
        }
    }
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.trees[i].name[0] != '\0') {
            printf("HANDLE type=logical_tree name=%s ops=%u\n",
                   g_ext.trees[i].name, g_ext.trees[i].value.op_count);
        }
    }
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.vectors[i].name[0] != '\0') {
            printf("HANDLE type=logical_vector name=%s logical_count=%u\n",
                   g_ext.vectors[i].name, g_ext.vectors[i].count);
        }
    }
    if (g_ext.allocator_mask != 0) {
        printf("HANDLE type=allocator_state active_mask=%u\n",
               g_ext.allocator_mask);
    }
}

void demo_extended_session_print_help(void)
{
    printf("Handler state:\n");
    printf("  ms_handler_init <handler> <hash|ac> <buckets> <secondary>\n");
    printf("  ms_set_hash_func <handler> <custom|null>\n");
    printf("  ms_add_rule|ms_remove_rule <handler> "
           "<alias key_len key|->... <rule_num>\n");
    printf("  ms_find_rule <handler> <alias> <key_len> <key|->\n");
    printf("  ms_compile_multi[_ext]|ms_add_exprs "
           "<handler> <arrays...> <elements>\n");
    printf("  ms_scan_expr|ms_compile_scan_ac_combinations "
           "<handler> <data> <len> <capacity>\n");
    printf("  ms_compile_eval_combinations <handler> "
           "<alias_csv> <alias_count> <capacity>\n");
    printf("  ms_open_handler_stream <handler> <stream>\n");
    printf("  ms_scan_expr_stream <stream> <data> <len> <capacity>\n");
    printf("  ms_scan_rule_stream <stream> <data> <len>\n");
    printf("  ms_scan_expr_stream_flush <stream> <capacity>\n");
    printf("  ms_reset_stream <stream>\n");
    printf("  ms_close_handler_stream <stream>\n");
    printf("  ms_handler_destroy <handler>\n");
    printf("Logical state:\n");
    printf("  ms_logical_parse <tree> <expression>\n");
    printf("  logical_tree_create_op <tree> <result_id> "
           "<not|and|or> <left> <right>\n");
    printf("  logical_vector_create <vector> <logical_count>\n");
    printf("  ms_logical_set <vector> <key> <0|1>\n");
    printf("  ms_logical_get <vector> <key>\n");
    printf("  ms_logical_clear <vector>\n");
    printf("  ms_logical_eval|ms_logical_eval_inplace <tree> <vector>\n");
    printf("  logical_vector_free <vector>\n");
    printf("  ms_logical_tree_destroy <tree>\n");
    printf("Allocator state:\n");
    printf("  ms_set_*allocator <custom|default> "
           "<custom|default> <custom|default>\n");
    printf("  allocator_stats [reset]\n");
}

int demo_extended_session_cleanup(void)
{
    int leaked = 0;
    int i;

    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.streams[i].name[0] != '\0') {
            int ret = ms_close_handler_stream(g_ext.streams[i].value);
            printf("SESSION_CLEANUP type=handler_stream handle=%s "
                   "ret=%d(%s)\n", g_ext.streams[i].name,
                   ret, demo_ret_name(ret));
            (void)memset(&g_ext.streams[i], 0, sizeof(g_ext.streams[i]));
            leaked = 1;
        }
    }
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.handlers[i].name[0] != '\0') {
            int ret = ms_handler_destroy(&g_ext.handlers[i].value);
            printf("SESSION_CLEANUP type=handler handle=%s ret=%d(%s)\n",
                   g_ext.handlers[i].name, ret, demo_ret_name(ret));
            (void)memset(&g_ext.handlers[i], 0, sizeof(g_ext.handlers[i]));
            leaked = 1;
        }
    }
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.trees[i].name[0] != '\0') {
            ms_logical_tree_destroy(&g_ext.trees[i].value);
            printf("SESSION_CLEANUP type=logical_tree handle=%s "
                   "ret=0(%s)\n", g_ext.trees[i].name,
                   demo_ret_name(MS_SUCCESS));
            (void)memset(&g_ext.trees[i], 0, sizeof(g_ext.trees[i]));
            leaked = 1;
        }
    }
    for (i = 0; i < EXT_MAX_HANDLES; i++) {
        if (g_ext.vectors[i].name[0] != '\0') {
            free(g_ext.vectors[i].value);
            printf("SESSION_CLEANUP type=logical_vector handle=%s "
                   "ret=0(%s)\n", g_ext.vectors[i].name,
                   demo_ret_name(MS_SUCCESS));
            (void)memset(&g_ext.vectors[i], 0, sizeof(g_ext.vectors[i]));
            leaked = 1;
        }
    }
    if (g_ext.allocator_mask != 0) {
        (void)ms_set_allocator(NULL, NULL, NULL);
        printf("SESSION_CLEANUP type=allocator active_mask=%u "
               "state=RESTORED_DEFAULT\n", g_ext.allocator_mask);
        g_ext.allocator_mask = 0;
        leaked = 1;
    }
    return leaked;
}

void demo_extended_session_reset(void)
{
    (void)ms_set_allocator(NULL, NULL, NULL);
    (void)memset(&g_ext, 0, sizeof(g_ext));
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Stateful, fully manual database API session.
 */
#include "api_session_demo.h"
#include "api_session_extended_demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo_common.h"
#include "ms.h"
#include "ms_database.h"
#include "ms_rule.h"
#include "ms_runtime.h"

#define SESSION_MAX_HANDLES 64
#define SESSION_NAME_SIZE 32
#define SESSION_LINE_SIZE 8192
#define SESSION_MAX_ARGS 1024
#define SESSION_MAX_CAPACITY 65536u

typedef struct {
    char name[SESSION_NAME_SIZE];
    ms_database_t *value;
} session_db_t;

typedef struct {
    char name[SESSION_NAME_SIZE];
    ms_scratch_t *value;
    int db_slot;
} session_scratch_t;

typedef struct {
    char name[SESSION_NAME_SIZE];
    ms_stream_t *value;
    int db_slot;
} session_stream_t;

typedef struct {
    session_db_t dbs[SESSION_MAX_HANDLES];
    session_scratch_t scratches[SESSION_MAX_HANDLES];
    session_stream_t streams[SESSION_MAX_HANDLES];
} session_state_t;

static int valid_name(const char *name)
{
    size_t i;
    size_t length = name == NULL ? 0 : strlen(name);

    if (length == 0 || length >= SESSION_NAME_SIZE) return 0;
    for (i = 0; i < length; i++) {
        char ch = name[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) {
            return 0;
        }
    }
    return 1;
}

static int find_db(const session_state_t *state, const char *name)
{
    int i;
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->dbs[i].value != NULL &&
            strcmp(state->dbs[i].name, name) == 0) return i;
    }
    return -1;
}

static int find_scratch(const session_state_t *state, const char *name)
{
    int i;
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->scratches[i].value != NULL &&
            strcmp(state->scratches[i].name, name) == 0) return i;
    }
    return -1;
}

static int find_stream(const session_state_t *state, const char *name)
{
    int i;
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->streams[i].value != NULL &&
            strcmp(state->streams[i].name, name) == 0) return i;
    }
    return -1;
}

static int free_db_slot(const session_state_t *state)
{
    int i;
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->dbs[i].value == NULL) return i;
    }
    return -1;
}

static int free_scratch_slot(const session_state_t *state)
{
    int i;
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->scratches[i].value == NULL) return i;
    }
    return -1;
}

static int free_stream_slot(const session_state_t *state)
{
    int i;
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->streams[i].value == NULL) return i;
    }
    return -1;
}

static int name_in_use(const session_state_t *state, const char *name)
{
    return find_db(state, name) >= 0 || find_scratch(state, name) >= 0 ||
           find_stream(state, name) >= 0 ||
           demo_extended_session_name_in_use(name);
}

static int database_session_name_in_use(const session_state_t *state,
                                        const char *name)
{
    return find_db(state, name) >= 0 || find_scratch(state, name) >= 0 ||
           find_stream(state, name) >= 0;
}

static void store_name(char destination[SESSION_NAME_SIZE], const char *name)
{
    (void)snprintf(destination, SESSION_NAME_SIZE, "%s", name);
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

static int command_compile(session_state_t *state, int argc, char **argv,
                           int extended)
{
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    ms_database_t *db = NULL;
    demo_expr_spec_t spec = {0};
    int slot;
    int ret;

    if (argc < 9 || !valid_name(argv[1]) ||
        demo_parse_matcher(argv[2], &type) != 0 ||
        demo_parse_u32(argv[3], &buckets) != 0 ||
        demo_parse_u32(argv[4], &secondary) != 0) {
        printf("ERROR stage=CLI usage=%s <db_handle> <hash|ac> <buckets> "
               "<secondary> <arrays...> <elements>\n", argv[0]);
        return 1;
    }
    if (name_in_use(state, argv[1])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[1]);
        return 1;
    }
    slot = free_db_slot(state);
    if (slot < 0) {
        printf("ERROR stage=STATE type=database reason=HANDLE_TABLE_FULL\n");
        return 1;
    }
    if (demo_parse_expr_spec(argc - 5, argv + 5, extended, 0, &spec) != 0) {
        return 1;
    }
    ret = extended ?
          ms_compile_database_multi_ext(&db, type, buckets, secondary,
                                        spec.expressions, spec.flags, spec.ids,
                                        spec.ext, spec.count) :
          ms_compile_database_multi(&db, type, buckets, secondary,
                                    spec.expressions, spec.flags, spec.ids,
                                    spec.count);
    demo_free_expr_spec(&spec);
    if (ret != MS_SUCCESS || db == NULL) {
        printf("ERROR stage=API api=%s ret=%d(%s) output.db=%s\n",
               argv[0], ret, demo_ret_name(ret),
               db == NULL ? "NULL" : "NON_NULL");
        if (db != NULL) (void)ms_free_database(db);
        return 1;
    }
    state->dbs[slot].value = db;
    store_name(state->dbs[slot].name, argv[1]);
    printf("OK api=%s ret=0(MS_SUCCESS) db=%s state=CREATED\n",
           argv[0], argv[1]);
    return 0;
}

static int command_create_db(session_state_t *state, int argc, char **argv)
{
    ms_matcher_type type;
    uint32_t buckets;
    uint32_t secondary;
    ms_database_t *db = NULL;
    int slot;
    int ret;

    if (argc != 5 || !valid_name(argv[1]) ||
        demo_parse_matcher(argv[2], &type) != 0 ||
        demo_parse_u32(argv[3], &buckets) != 0 ||
        demo_parse_u32(argv[4], &secondary) != 0) {
        printf("ERROR stage=CLI usage=ms_create_database <db_handle> "
               "<hash|ac> <buckets> <secondary>\n");
        return 1;
    }
    if (name_in_use(state, argv[1])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[1]);
        return 1;
    }
    slot = free_db_slot(state);
    if (slot < 0) return 1;
    ret = ms_create_database(&db, type, buckets, secondary);
    if (ret != MS_SUCCESS || db == NULL) {
        printf("ERROR stage=API api=ms_create_database ret=%d(%s) "
               "output.db=%s\n", ret, demo_ret_name(ret),
               db == NULL ? "NULL" : "NON_NULL");
        if (db != NULL) (void)ms_free_database(db);
        return 1;
    }
    state->dbs[slot].value = db;
    store_name(state->dbs[slot].name, argv[1]);
    printf("OK api=ms_create_database ret=0(MS_SUCCESS) db=%s state=CREATED\n",
           argv[1]);
    return 0;
}

static int has_db_children(const session_state_t *state, int db_slot)
{
    int i;
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if ((state->scratches[i].value != NULL &&
             state->scratches[i].db_slot == db_slot) ||
            (state->streams[i].value != NULL &&
             state->streams[i].db_slot == db_slot)) return 1;
    }
    return 0;
}

static int command_free_db(session_state_t *state, int argc, char **argv)
{
    int slot;
    int ret;

    if (argc != 2 || (slot = find_db(state, argv[1])) < 0) {
        printf("ERROR stage=STATE api=ms_free_database handle=%s "
               "reason=NOT_FOUND\n", argc > 1 ? argv[1] : "-");
        return 1;
    }
    if (has_db_children(state, slot)) {
        printf("ERROR stage=STATE api=ms_free_database db=%s "
               "reason=LIVE_SCRATCH_OR_STREAM\n", argv[1]);
        return 1;
    }
    ret = ms_free_database(state->dbs[slot].value);
    if (ret != MS_SUCCESS) {
        printf("ERROR stage=API api=ms_free_database ret=%d(%s) db=%s\n",
               ret, demo_ret_name(ret), argv[1]);
        return 1;
    }
    (void)memset(&state->dbs[slot], 0, sizeof(state->dbs[slot]));
    printf("OK api=ms_free_database ret=0(MS_SUCCESS) db=%s state=RELEASED\n",
           argv[1]);
    return 0;
}

static int command_alloc_scratch(session_state_t *state, int argc, char **argv)
{
    ms_scratch_t *scratch = NULL;
    int db_slot;
    int slot;
    int ret;

    if (argc != 3 || (db_slot = find_db(state, argv[1])) < 0 ||
        !valid_name(argv[2])) {
        printf("ERROR stage=STATE usage=ms_alloc_scratch <db_handle> "
               "<scratch_handle> reason=INVALID_OR_MISSING_DB\n");
        return 1;
    }
    if (name_in_use(state, argv[2])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[2]);
        return 1;
    }
    slot = free_scratch_slot(state);
    if (slot < 0) return 1;
    printf("PRECONDITION api=ms_alloc_scratch source=EXISTING "
           "database=%s status=READY\n", argv[1]);
    ret = ms_alloc_scratch(state->dbs[db_slot].value, &scratch);
    if (ret != MS_SUCCESS || scratch == NULL) {
        printf("ERROR stage=API api=ms_alloc_scratch ret=%d(%s) "
               "output.scratch=%s\n", ret, demo_ret_name(ret),
               scratch == NULL ? "NULL" : "NON_NULL");
        if (scratch != NULL) (void)ms_free_scratch(scratch);
        return 1;
    }
    state->scratches[slot].value = scratch;
    state->scratches[slot].db_slot = db_slot;
    store_name(state->scratches[slot].name, argv[2]);
    printf("OK api=ms_alloc_scratch ret=0(MS_SUCCESS) db=%s scratch=%s "
           "state=CREATED\n", argv[1], argv[2]);
    return 0;
}

static int command_free_scratch(session_state_t *state, int argc, char **argv)
{
    int slot;
    int i;
    int ret;

    if (argc != 2 || (slot = find_scratch(state, argv[1])) < 0) {
        printf("ERROR stage=STATE api=ms_free_scratch handle=%s "
               "reason=NOT_FOUND\n", argc > 1 ? argv[1] : "-");
        return 1;
    }
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->streams[i].value != NULL &&
            state->streams[i].db_slot == state->scratches[slot].db_slot) {
            printf("ERROR stage=STATE api=ms_free_scratch scratch=%s "
                   "reason=LIVE_STREAM_FOR_DATABASE\n", argv[1]);
            return 1;
        }
    }
    ret = ms_free_scratch(state->scratches[slot].value);
    if (ret != MS_SUCCESS) {
        printf("ERROR stage=API api=ms_free_scratch ret=%d(%s) scratch=%s\n",
               ret, demo_ret_name(ret), argv[1]);
        return 1;
    }
    (void)memset(&state->scratches[slot], 0,
                 sizeof(state->scratches[slot]));
    printf("OK api=ms_free_scratch ret=0(MS_SUCCESS) scratch=%s "
           "state=RELEASED\n", argv[1]);
    return 0;
}

static int command_scan_db(session_state_t *state, int argc, char **argv)
{
    uint32_t len;
    uint32_t capacity;
    uint32_t count = 0;
    uint32_t *ids;
    int db_slot;
    int scratch_slot;
    int ret;

    if (argc != 6) {
        printf("ERROR stage=CLI api=ms_scan_database reason=ARG_COUNT "
               "expected=5 actual=%d\n", argc - 1);
        return 1;
    }
    db_slot = find_db(state, argv[1]);
    if (db_slot < 0) {
        printf("ERROR stage=STATE api=ms_scan_database type=database "
               "handle=%s reason=NOT_FOUND\n", argv[1]);
        return 1;
    }
    if (demo_parse_u32(argv[3], &len) != 0) {
        printf("ERROR stage=CLI api=ms_scan_database field=len "
               "reason=NOT_UINT32 value=%s\n", argv[3]);
        return 1;
    }
    if (len > strlen(argv[2])) {
        printf("ERROR stage=CLI api=ms_scan_database field=len value=%u "
               "data_bytes=%zu reason=OUT_OF_RANGE\n",
               len, strlen(argv[2]));
        return 1;
    }
    scratch_slot = find_scratch(state, argv[4]);
    if (scratch_slot < 0) {
        printf("ERROR stage=STATE api=ms_scan_database type=scratch "
               "handle=%s reason=NOT_FOUND\n", argv[4]);
        return 1;
    }
    if (state->scratches[scratch_slot].db_slot != db_slot) {
        printf("ERROR stage=STATE api=ms_scan_database scratch=%s db=%s "
               "reason=OWNER_MISMATCH actual_db=%s\n",
               argv[4], argv[1],
               state->dbs[state->scratches[scratch_slot].db_slot].name);
        return 1;
    }
    if (demo_parse_u32(argv[5], &capacity) != 0 ||
        capacity > SESSION_MAX_CAPACITY) {
        printf("ERROR stage=CLI api=ms_scan_database field=capacity "
               "reason=OUT_OF_RANGE value=%s max=%u\n",
               argv[5], SESSION_MAX_CAPACITY);
        return 1;
    }
    ids = capacity == 0 ? NULL : (uint32_t *)calloc(capacity, sizeof(*ids));
    if (capacity != 0 && ids == NULL) return 1;
    printf("PRECONDITION api=ms_scan_database source=EXISTING "
           "database=%s scratch=%s status=READY\n", argv[1], argv[4]);
    ret = ms_scan_database(state->dbs[db_slot].value,
                           (const uint8_t *)argv[2], len,
                           state->scratches[scratch_slot].value,
                           ids, &count, capacity);
    printf("%s api=ms_scan_database ret=%d(%s) db=%s scratch=%s",
           ret == MS_SUCCESS ? "OK" :
           ret == MS_FIND_FAIL ? "NO_MATCH" : "ERROR",
           ret, demo_ret_name(ret), argv[1], argv[4]);
    print_ids(ids, count, capacity);
    free(ids);
    return ret == MS_SUCCESS || ret == MS_FIND_FAIL ? 0 : 1;
}

static int command_open_stream(session_state_t *state, int argc, char **argv)
{
    ms_stream_t *stream = NULL;
    int db_slot;
    int slot;
    int ret;

    if (argc != 3 || (db_slot = find_db(state, argv[1])) < 0 ||
        !valid_name(argv[2])) {
        printf("ERROR stage=STATE usage=ms_open_stream <db> <stream> "
               "reason=INVALID_OR_MISSING_DB\n");
        return 1;
    }
    if (name_in_use(state, argv[2])) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n", argv[2]);
        return 1;
    }
    slot = free_stream_slot(state);
    if (slot < 0) return 1;
    printf("PRECONDITION api=ms_open_stream source=EXISTING "
           "database=%s status=READY\n", argv[1]);
    ret = ms_open_stream(state->dbs[db_slot].value, &stream);
    if (ret != MS_SUCCESS || stream == NULL) {
        printf("ERROR stage=API api=ms_open_stream ret=%d(%s) "
               "output.stream=%s\n", ret, demo_ret_name(ret),
               stream == NULL ? "NULL" : "NON_NULL");
        return 1;
    }
    state->streams[slot].value = stream;
    state->streams[slot].db_slot = db_slot;
    store_name(state->streams[slot].name, argv[2]);
    printf("OK api=ms_open_stream ret=0(MS_SUCCESS) db=%s stream=%s "
           "state=CREATED\n", argv[1], argv[2]);
    return 0;
}

static int stream_output_command(session_state_t *state, int argc, char **argv,
                                 int operation)
{
    uint32_t len = 0;
    uint32_t capacity;
    uint32_t count = 0;
    uint32_t *ids;
    int stream_slot;
    int scratch_slot;
    int expected = operation == 0 ? 6 : 4;
    int ret;

    if (argc != expected) {
        printf("ERROR stage=CLI api=%s reason=ARG_COUNT expected=%d actual=%d\n",
               argv[0], expected - 1, argc - 1);
        return 1;
    }
    stream_slot = find_stream(state, argv[1]);
    if (stream_slot < 0) {
        printf("ERROR stage=STATE api=%s type=stream handle=%s "
               "reason=NOT_FOUND\n", argv[0], argv[1]);
        return 1;
    }
    if (operation == 0) {
        if (demo_parse_u32(argv[3], &len) != 0) {
            printf("ERROR stage=CLI api=%s field=len reason=NOT_UINT32 "
                   "value=%s\n", argv[0], argv[3]);
            return 1;
        }
        if (len > strlen(argv[2])) {
            printf("ERROR stage=CLI api=%s field=len value=%u data_bytes=%zu "
                   "reason=OUT_OF_RANGE\n", argv[0], len, strlen(argv[2]));
            return 1;
        }
    }
    scratch_slot = find_scratch(state, argv[operation == 0 ? 4 : 2]);
    if (scratch_slot < 0) {
        printf("ERROR stage=STATE api=%s type=scratch handle=%s "
               "reason=NOT_FOUND\n", argv[0],
               argv[operation == 0 ? 4 : 2]);
        return 1;
    }
    if (state->scratches[scratch_slot].db_slot !=
        state->streams[stream_slot].db_slot) {
        printf("ERROR stage=STATE api=%s stream=%s scratch=%s "
               "reason=OWNER_MISMATCH\n", argv[0], argv[1],
               argv[operation == 0 ? 4 : 2]);
        return 1;
    }
    if (demo_parse_u32(argv[operation == 0 ? 5 : 3], &capacity) != 0 ||
        capacity > SESSION_MAX_CAPACITY) {
        printf("ERROR stage=CLI api=%s field=capacity reason=OUT_OF_RANGE "
               "value=%s max=%u\n", argv[0],
               argv[operation == 0 ? 5 : 3], SESSION_MAX_CAPACITY);
        return 1;
    }
    ids = capacity == 0 ? NULL : (uint32_t *)calloc(capacity, sizeof(*ids));
    if (capacity != 0 && ids == NULL) return 1;
    printf("PRECONDITION api=%s source=EXISTING stream=%s scratch=%s "
           "status=READY\n", argv[0], argv[1],
           argv[operation == 0 ? 4 : 2]);
    if (operation == 0) {
        ret = ms_scan_stream(state->streams[stream_slot].value,
                             (const uint8_t *)argv[2], len,
                             state->scratches[scratch_slot].value,
                             ids, &count, capacity);
    } else if (operation == 1) {
        ret = ms_flush_stream(state->streams[stream_slot].value,
                              state->scratches[scratch_slot].value,
                              ids, &count, capacity);
    } else {
        ret = ms_close_stream(state->streams[stream_slot].value,
                              state->scratches[scratch_slot].value,
                              ids, &count, capacity);
    }
    printf("%s api=%s ret=%d(%s) stream=%s scratch=%s",
           ret == MS_SUCCESS ? "OK" :
           ret == MS_FIND_FAIL ? "NO_MATCH" : "ERROR",
           argv[0], ret, demo_ret_name(ret), argv[1],
           argv[operation == 0 ? 4 : 2]);
    print_ids(ids, count, capacity);
    free(ids);
    if (operation == 2 && ret != MS_INVALID_PARA) {
        (void)memset(&state->streams[stream_slot], 0,
                     sizeof(state->streams[stream_slot]));
    }
    return ret == MS_SUCCESS || ret == MS_FIND_FAIL ? 0 : 1;
}

static int command_reset_stream(session_state_t *state, int argc, char **argv)
{
    int slot;
    int ret;

    if (argc != 2 || (slot = find_stream(state, argv[1])) < 0) {
        printf("ERROR stage=STATE api=ms_reset_stream handle=%s "
               "reason=NOT_FOUND\n", argc > 1 ? argv[1] : "-");
        return 1;
    }
    ret = ms_reset_stream(state->streams[slot].value);
    printf("%s api=ms_reset_stream ret=%d(%s) stream=%s\n",
           ret == MS_SUCCESS ? "OK" : "ERROR",
           ret, demo_ret_name(ret), argv[1]);
    return ret == MS_SUCCESS ? 0 : 1;
}

static int command_find_rule(session_state_t *state, int argc, char **argv)
{
    ms_rule_t rule = {0};
    int db_slot;
    int ret;

    if (argc != 5 || (db_slot = find_db(state, argv[1])) < 0 ||
        demo_parse_u32(argv[2], &rule.alias) != 0 ||
        demo_parse_u32(argv[3], &rule.key_len) != 0 ||
        (strcmp(argv[4], "-") != 0 && rule.key_len > strlen(argv[4]))) {
        printf("ERROR stage=CLI_OR_STATE usage=ms_find_database_rule <db> "
               "<alias> <key_len> <key|->\n");
        return 1;
    }
    rule.key = strcmp(argv[4], "-") == 0 ? NULL : (uint8_t *)argv[4];
    printf("PRECONDITION api=ms_find_database_rule source=EXISTING "
           "database=%s status=READY\n", argv[1]);
    ret = ms_find_database_rule(state->dbs[db_slot].value, &rule);
    printf("%s api=ms_find_database_rule ret=%d(%s) db=%s alias=%u\n",
           ret == MS_SUCCESS ? "OK" :
           ret == MS_FIND_FAIL ? "NO_MATCH" : "ERROR",
           ret, demo_ret_name(ret), argv[1], rule.alias);
    return ret == MS_SUCCESS || ret == MS_FIND_FAIL ? 0 : 1;
}

static int command_remove_rules(session_state_t *state, int argc, char **argv)
{
    ms_rule_t *rules;
    uint32_t count;
    uint32_t rule_num;
    uint32_t i;
    int db_slot;
    int ret;

    if (argc < 6 || (db_slot = find_db(state, argv[1])) < 0 ||
        ((uint32_t)(argc - 3) % 3u) != 0 ||
        demo_parse_u32(argv[argc - 1], &rule_num) != 0) {
        printf("ERROR stage=CLI_OR_STATE usage=ms_remove_database_rule <db> "
               "<alias key_len key|->... <rule_num>\n");
        return 1;
    }
    count = (uint32_t)(argc - 3) / 3u;
    if (count != rule_num) {
        printf("ERROR stage=CLI code=COUNT_MISMATCH serialized=%u "
               "rule_num=%u\n", count, rule_num);
        return 1;
    }
    rules = (ms_rule_t *)calloc(count, sizeof(*rules));
    if (rules == NULL) return 1;
    for (i = 0; i < count; i++) {
        uint32_t offset = 2u + i * 3u;
        if (demo_parse_u32(argv[offset], &rules[i].alias) != 0 ||
            demo_parse_u32(argv[offset + 1u], &rules[i].key_len) != 0 ||
            (strcmp(argv[offset + 2u], "-") != 0 &&
             rules[i].key_len > strlen(argv[offset + 2u]))) {
            printf("ERROR stage=CLI code=INVALID_RULE index=%u\n", i);
            free(rules);
            return 1;
        }
        rules[i].key = strcmp(argv[offset + 2u], "-") == 0 ? NULL :
                       (uint8_t *)argv[offset + 2u];
    }
    printf("PRECONDITION api=ms_remove_database_rule source=EXISTING "
           "database=%s status=READY\n", argv[1]);
    ret = ms_remove_database_rule(state->dbs[db_slot].value,
                                  rules, rule_num);
    free(rules);
    printf("%s api=ms_remove_database_rule ret=%d(%s) db=%s rule_num=%u\n",
           ret == MS_SUCCESS ? "OK" : "ERROR",
           ret, demo_ret_name(ret), argv[1], rule_num);
    return ret == MS_SUCCESS ? 0 : 1;
}

static void list_handles(const session_state_t *state)
{
    int i;
    printf("TYPE\tHANDLE\tOWNER\n");
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->dbs[i].value != NULL) {
            printf("database\t%s\t-\n", state->dbs[i].name);
        }
        if (state->scratches[i].value != NULL) {
            printf("scratch\t%s\t%s\n", state->scratches[i].name,
                   state->dbs[state->scratches[i].db_slot].name);
        }
        if (state->streams[i].value != NULL) {
            printf("stream\t%s\t%s\n", state->streams[i].name,
                   state->dbs[state->streams[i].db_slot].name);
        }
    }
    demo_extended_session_list_handles();
}

void demo_print_api_session_help(const char *prog)
{
    printf("用法：%s session [command-file]\n", prog);
    printf("每行一个命令；对象保存在当前进程，直到显式 free/close。\n");
    printf("  ms_create_database <db> <hash|ac> <buckets> <secondary>\n");
    printf("  ms_compile_database_multi <db> <type> <buckets> <secondary> "
           "<expressions[N]> <flags[N]> <ids[N]> <elements>\n");
    printf("  ms_compile_database_multi_ext <db> <type> <buckets> <secondary> "
           "<expressions[N]> <flags[N]> <ids[N]> "
           "<ext flags/min/max[N]> <elements>\n");
    printf("  ms_alloc_scratch <db> <scratch>\n");
    printf("  ms_scan_database <db> <data> <len> <scratch> <capacity>\n");
    printf("  ms_find_database_rule <db> <alias> <key_len> <key|->\n");
    printf("  ms_remove_database_rule <db> "
           "<alias key_len key|->... <rule_num>\n");
    printf("  ms_open_stream <db> <stream>\n");
    printf("  ms_scan_stream <stream> <data> <len> <scratch> <capacity>\n");
    printf("  ms_flush_stream <stream> <scratch> <capacity>\n");
    printf("  ms_reset_stream <stream>\n");
    printf("  ms_close_stream <stream> <scratch> <capacity>\n");
    printf("  ms_free_scratch <scratch>\n");
    printf("  ms_free_database <db>\n");
    printf("  handles | help | exit\n");
    demo_extended_session_print_help();
}

static int tokenize(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *read = line;
    char *write = line;

    while (*read != '\0') {
        char quote = '\0';
        int quote_closed = 0;
        int comment;

        while (*read == ' ' || *read == '\t' || *read == '\r' ||
               *read == '\n') read++;
        if (*read == '\0' || *read == '#') break;
        if (argc == max_args) return -1;
        argv[argc++] = write;
        if (*read == '\'' || *read == '"') quote = *read++;
        while (*read != '\0') {
            if (quote != '\0') {
                if (*read == quote) {
                    read++;
                    quote_closed = 1;
                    break;
                }
            } else if (*read == ' ' || *read == '\t' || *read == '\r' ||
                       *read == '\n' || *read == '#') {
                break;
            }
            *write++ = *read++;
        }
        if (quote != '\0' && !quote_closed) return -1;
        comment = *read == '#';
        while (*read == ' ' || *read == '\t' || *read == '\r' ||
               *read == '\n') read++;
        *write++ = '\0';
        if (comment) break;
    }
    return argc;
}

static int dispatch(session_state_t *state, int argc, char **argv)
{
    int extended_ret;
    const char *extended_output_name = NULL;

    if (strcmp(argv[0], "ms_create_database") == 0) {
        if (argc == 5) return command_create_db(state, argc, argv);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_compile_database_multi") == 0) {
        ms_matcher_type ignored;
        if (argc > 1 && demo_parse_matcher(argv[1], &ignored) == 0) {
            goto automatic;
        }
        return command_compile(state, argc, argv, 0);
    }
    if (strcmp(argv[0], "ms_compile_database_multi_ext") == 0) {
        ms_matcher_type ignored;
        if (argc > 1 && demo_parse_matcher(argv[1], &ignored) == 0) {
            goto automatic;
        }
        return command_compile(state, argc, argv, 1);
    }
    if (strcmp(argv[0], "ms_free_database") == 0) {
        if (argc == 2) return command_free_db(state, argc, argv);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_alloc_scratch") == 0) {
        if (argc == 3) return command_alloc_scratch(state, argc, argv);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_free_scratch") == 0) {
        if (argc == 2) return command_free_scratch(state, argc, argv);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_scan_database") == 0) {
        if (argc == 6) return command_scan_db(state, argc, argv);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_open_stream") == 0) {
        if (argc == 3) return command_open_stream(state, argc, argv);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_scan_stream") == 0) {
        if (argc == 6) return stream_output_command(state, argc, argv, 0);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_flush_stream") == 0) {
        if (argc == 4) return stream_output_command(state, argc, argv, 1);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_close_stream") == 0) {
        if (argc == 4) return stream_output_command(state, argc, argv, 2);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_reset_stream") == 0) {
        if (argc == 2 && find_stream(state, argv[1]) >= 0) {
            return command_reset_stream(state, argc, argv);
        }
        extended_ret = demo_extended_session_dispatch(argc, argv);
        if (extended_ret >= 0) return extended_ret;
        goto automatic;
    }
    if (strcmp(argv[0], "ms_find_database_rule") == 0) {
        if (argc == 5) return command_find_rule(state, argc, argv);
        goto automatic;
    }
    if (strcmp(argv[0], "ms_remove_database_rule") == 0) {
        int i;
        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--") == 0) goto automatic;
        }
        return command_remove_rules(state, argc, argv);
    }
    if (strcmp(argv[0], "handles") == 0) {
        list_handles(state);
        return 0;
    }
    if ((strcmp(argv[0], "ms_handler_init") == 0 ||
         strcmp(argv[0], "ms_logical_parse") == 0 ||
         strcmp(argv[0], "logical_tree_create_op") == 0 ||
         strcmp(argv[0], "logical_vector_create") == 0) && argc > 1) {
        extended_output_name = argv[1];
    } else if (strcmp(argv[0], "ms_open_handler_stream") == 0 && argc > 2) {
        extended_output_name = argv[2];
    }
    if (extended_output_name != NULL &&
        database_session_name_in_use(state, extended_output_name)) {
        printf("ERROR stage=STATE handle=%s reason=ALREADY_EXISTS\n",
               extended_output_name);
        return 1;
    }
    extended_ret = demo_extended_session_dispatch(argc, argv);
    if (extended_ret >= 0) return extended_ret;
    if (strncmp(argv[0], "ms_", 3) == 0) {
automatic:
        printf("PRECONDITION api=%s source=AUTO reason=NO_STORED_HANDLE_FORM "
               "status=PREPARING\n", argv[0]);
        return demo_run_api_cli(argv[0], argc - 1, argv + 1);
    }
    printf("ERROR stage=CLI command=%s reason=UNKNOWN_COMMAND\n", argv[0]);
    return 1;
}

static int cleanup_session(session_state_t *state)
{
    int leaked = 0;
    int i;

    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->streams[i].value != NULL) {
            int j;
            int ret = MS_INVALID_PARA;
            for (j = 0; j < SESSION_MAX_HANDLES; j++) {
                if (state->scratches[j].value != NULL &&
                    state->scratches[j].db_slot == state->streams[i].db_slot) {
                    uint32_t id = 0;
                    uint32_t count = 0;
                    ret = ms_close_stream(state->streams[i].value,
                                          state->scratches[j].value,
                                          &id, &count, 1);
                    break;
                }
            }
            printf("SESSION_CLEANUP type=stream handle=%s ret=%d(%s)\n",
                   state->streams[i].name, ret, demo_ret_name(ret));
            leaked = 1;
            (void)memset(&state->streams[i], 0, sizeof(state->streams[i]));
        }
    }
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->scratches[i].value != NULL) {
            int ret = ms_free_scratch(state->scratches[i].value);
            printf("SESSION_CLEANUP type=scratch handle=%s ret=%d(%s)\n",
                   state->scratches[i].name, ret, demo_ret_name(ret));
            leaked = 1;
            (void)memset(&state->scratches[i], 0,
                         sizeof(state->scratches[i]));
        }
    }
    for (i = 0; i < SESSION_MAX_HANDLES; i++) {
        if (state->dbs[i].value != NULL) {
            int ret = ms_free_database(state->dbs[i].value);
            printf("SESSION_CLEANUP type=database handle=%s ret=%d(%s)\n",
                   state->dbs[i].name, ret, demo_ret_name(ret));
            leaked = 1;
            (void)memset(&state->dbs[i], 0, sizeof(state->dbs[i]));
        }
    }
    if (demo_extended_session_cleanup() != 0) leaked = 1;
    return leaked;
}

int demo_run_api_session(const char *script_path)
{
    session_state_t state = {0};
    FILE *input = stdin;
    char line[SESSION_LINE_SIZE];
    unsigned int line_number = 0;
    int interactive = script_path == NULL;
    int failures = 0;
    int exit_requested = 0;

    demo_extended_session_reset();
    if (!interactive) {
        input = fopen(script_path, "r");
        if (input == NULL) {
            printf("ERROR stage=SESSION script=%s reason=OPEN_FAILED\n",
                   script_path);
            return 1;
        }
    } else {
        printf("Megascan manual API session. 输入 help 查看命令。\n");
    }
    while (!exit_requested) {
        char *argv[SESSION_MAX_ARGS];
        int argc;

        if (interactive) {
            printf("ms> ");
            (void)fflush(stdout);
        }
        if (fgets(line, sizeof(line), input) == NULL) break;
        line_number++;
        argc = tokenize(line, argv, SESSION_MAX_ARGS);
        if (argc < 0) {
            printf("ERROR stage=CLI line=%u reason=TOKENIZE_FAILED\n",
                   line_number);
            failures++;
            continue;
        }
        if (argc == 0) continue;
        if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
            exit_requested = 1;
        } else if (strcmp(argv[0], "help") == 0) {
            demo_print_api_session_help("megascan_demo");
        } else if (dispatch(&state, argc, argv) != 0) {
            printf("COMMAND_FAILED line=%u\n", line_number);
            failures++;
        }
    }
    if (!interactive) (void)fclose(input);
    if (cleanup_session(&state) != 0) {
        printf("SESSION_FAILED reason=UNRELEASED_HANDLES\n");
        failures++;
    }
    printf("SESSION_RESULT commands_failed=%d\n", failures);
    return failures == 0 ? 0 : 1;
}

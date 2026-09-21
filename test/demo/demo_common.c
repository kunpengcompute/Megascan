/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include "demo_common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void demo_print_match_events(const ms_scratch_t *scratch)
{
    ms_match_event_t *events = NULL;
    uint32_t count = 0;
    uint32_t i;
    int ret;

    ret = ms_get_match_events(scratch, NULL, &count, 0);
    if (ret == MS_SUCCESS && count != 0) {
        events = (ms_match_event_t *)calloc(count, sizeof(*events));
        if (events == NULL) {
            printf("plain_match_events ret=%d(%s)\n",
                   MS_MALLOC_FAIL, demo_ret_name(MS_MALLOC_FAIL));
            return;
        }
        ret = ms_get_match_events(scratch, events, &count, count);
    }
    if (ret != MS_SUCCESS) {
        printf("plain_match_events ret=%d(%s)\n", ret, demo_ret_name(ret));
        free(events);
        return;
    }

    printf("plain_match_count=%u matches=[", count);
    for (i = 0; i < count; i++) {
        printf("%s{id=%u,from=%llu,to=%llu}", i == 0 ? "" : ",",
               events[i].id, (unsigned long long)events[i].from,
               (unsigned long long)events[i].to);
    }
    printf("]\n");
    free(events);
}

int demo_same(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

int demo_parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL || text[0] == '-') return -1;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

int demo_parse_u64(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || value == NULL || text[0] == '-') return -1;
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return -1;
    *value = (uint64_t)parsed;
    return 0;
}

int demo_parse_matcher(const char *text, ms_matcher_type *type)
{
    if (type == NULL) return -1;
    if (demo_same(text, "hash") || demo_same(text, "HASH") ||
        demo_same(text, "1")) {
        *type = MS_MATCHER_TYPE_HASH;
        return 0;
    }
    if (demo_same(text, "ac") || demo_same(text, "AC") ||
        demo_same(text, "2")) {
        *type = MS_MATCHER_TYPE_AC;
        return 0;
    }
    return -1;
}

const char *demo_ret_name(int ret)
{
    switch (ret) {
        case MS_SUCCESS:
            return "MS_SUCCESS";
        case MS_INVALID_PARA:
            return "MS_INVALID_PARA";
        case MS_MALLOC_FAIL:
            return "MS_MALLOC_FAIL";
        case MS_BUILT_ALREADY:
            return "MS_BUILT_ALREADY";
        case MS_FIND_FAIL:
            return "MS_FIND_FAIL";
        case MS_INIT_RWLOCK_FAIL:
            return "MS_INIT_RWLOCK_FAIL";
        case MS_MEMCPY_FAIL:
            return "MS_MEMCPY_FAIL";
        case MS_NOT_SUPPORTED:
            return "MS_NOT_SUPPORTED";
        case MS_STREAM_STALE:
            return "MS_STREAM_STALE";
        default:
            return "未知返回码";
    }
}

int demo_result_from_ret(int ret)
{
    return ret == MS_SUCCESS || ret == MS_FIND_FAIL ? 0 : 1;
}

void demo_free_expr_spec(demo_expr_spec_t *spec)
{
    if (spec == NULL) return;
    free((void *)spec->expressions);
    free(spec->ids);
    free(spec->flags);
    free(spec->ext);
    (void)memset(spec, 0, sizeof(*spec));
}

int demo_parse_expr_spec(int argc, char **argv, int extended,
                         int ids_before_flags, demo_expr_spec_t *spec)
{
    uint32_t fields = extended ? 6u : 3u;
    uint32_t count;
    uint32_t elements;
    uint32_t i;

    if (spec == NULL || argc < (extended ? 7 : 4) ||
        ((uint32_t)(argc - 1) % fields) != 0) {
        printf("ERROR stage=CLI code=ARRAY_LAYOUT expected="
               "expressions[N],flags[N],ids[N]%s,elements\n",
               extended ? ",ext[N](flags/min/max)" : "");
        return -1;
    }
    count = (uint32_t)(argc - 1) / fields;
    if (demo_parse_u32(argv[argc - 1], &elements) != 0) {
        printf("ERROR stage=CLI field=elements reason=NOT_UINT32 value=%s\n",
               argv[argc - 1]);
        return -1;
    }
    if (elements != count) {
        printf("ERROR stage=CLI code=COUNT_MISMATCH serialized=%u "
               "elements=%u\n", count, elements);
        return -1;
    }
    spec->expressions = (const char **)calloc(count + 1u,
                                              sizeof(*spec->expressions));
    spec->flags = (uint32_t *)calloc(count, sizeof(*spec->flags));
    spec->ids = (uint32_t *)calloc(count, sizeof(*spec->ids));
    if (extended) {
        spec->ext = (ms_expr_ext_t *)calloc(count, sizeof(*spec->ext));
    }
    if (spec->expressions == NULL || spec->flags == NULL || spec->ids == NULL ||
        (extended && spec->ext == NULL)) {
        printf("ERROR stage=CLI code=NO_MEMORY\n");
        demo_free_expr_spec(spec);
        return -1;
    }
    for (i = 0; i < count; i++) {
        const char *flag_text = argv[count + i];
        const char *id_text = argv[count * 2u + i];
        uint32_t ext_offset = count * 3u + i * 3u;

        if (ids_before_flags) {
            const char *swap = flag_text;
            flag_text = id_text;
            id_text = swap;
        }
        spec->expressions[i] = argv[i];
        if (demo_parse_u32(flag_text, &spec->flags[i]) != 0 ||
            demo_parse_u32(id_text, &spec->ids[i]) != 0 ||
            (extended &&
             (demo_parse_u32(argv[ext_offset], &spec->ext[i].flags) != 0 ||
              demo_parse_u64(argv[ext_offset + 1u],
                             &spec->ext[i].min_offset) != 0 ||
              demo_parse_u64(argv[ext_offset + 2u],
                             &spec->ext[i].max_offset) != 0))) {
            printf("ERROR stage=CLI code=INVALID_ARRAY_VALUE index=%u\n", i);
            demo_free_expr_spec(spec);
            return -1;
        }
    }
    spec->count = count;
    return 0;
}

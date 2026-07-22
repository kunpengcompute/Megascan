/*
 * Preloaded-corpus benchmark for HASH, AC and AC combination.
 * stream-full times reset + one full-payload scan per query; stream close/flush
 * is kept outside the timed loop. Optional workers share the compiled matcher,
 * own their scratch/stream state, and are pinned to the requested CPU list.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include "ms.h"
#include "ms_handler.h"
#include "ms_rule.h"
#include "ms_runtime.h"

#define CORPUS_MAGIC 0x4d534243u
#define CORPUS_VERSION 1u
#define KIND_HASH 1u
#define KIND_AC 2u
#define KIND_ACCOMBO 3u
#define HASH_ADD_BATCH 65536u
#define OUTPUT_CAPACITY 64u

static void wait_for_attached_profiler(void)
{
    const char *enabled = getenv("MS_BENCH_PROFILE_STOP");

    if (enabled == NULL || strcmp(enabled, "1") != 0) {
        return;
    }
    fprintf(stderr, "[phase] profiler attach ready pid=%ld\n", (long)getpid());
    fflush(stderr);
    (void)raise(SIGSTOP);
    fprintf(stderr, "[phase] profiler attached; timed trials continue\n");
}

typedef enum {
    ALG_HASH = 1,
    ALG_AC = 2,
    ALG_ACCOMBO = 3,
    ALG_AC_LEGACY = 4,
    ALG_AC_ON_COMBO = 5,
    ALG_AC_DELETE = 6,
    ALG_ACCOMBO_DELETE = 7
} algorithm_t;

typedef enum {
    MODE_LOOKUP = 1,
    MODE_BLOCK = 2,
    MODE_STREAM = 3,
    MODE_BOTH = 4
} bench_mode_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t record_header_size;
    uint32_t query_count;
    uint32_t payload_size;
    uint32_t hit_count;
    uint32_t seed;
    uint32_t kind;
    uint32_t reserved;
} corpus_header_t;

typedef struct {
    uint32_t expected_ids;
    uint32_t expected_candidates;
    uint32_t stream_id;
    uint32_t reserved;
} corpus_record_header_t;

typedef struct {
    uint32_t src_ip;
    uint16_t src_port;
    uint8_t protocol;
    uint32_t dst_ip;
    uint16_t dst_port;
} tuple_t;

typedef struct {
    char **expressions;
    uint32_t *flags;
    uint32_t *ids;
    uint32_t count;
    uint32_t atomic_count;
    uint32_t combo_count;
} expression_set_t;

typedef struct {
    corpus_header_t header;
    uint8_t *records;
    size_t record_size;
} corpus_t;

typedef struct {
    uint64_t queries;
    uint64_t bytes;
    uint64_t matches;
    uint64_t expected_matches;
    uint64_t reported_ids;
    uint64_t expected_ids;
    uint64_t expected_candidates;
} scan_stats_t;

typedef struct {
    algorithm_t algorithm;
    const corpus_t *corpus;
    ms_handler_t *hash_handler;
    ms_handler_t *legacy_handler;
    const ms_database_t *db;
    ms_scratch_t *scratch;
    ms_stream_t *stream;
    const uint32_t *expected_ids_after_delete;
    const uint32_t *expected_output_ids_after_delete;
    uint32_t output_ids[OUTPUT_CAPACITY];
} scan_context_t;

typedef struct {
    ms_rule_t rule;
    uint32_t affected_id;
    uint32_t probe_query;
    uint32_t *expected_ids;
    uint32_t *expected_output_ids;
} dynamic_delete_plan_t;

typedef struct {
    scan_context_t context;
    bench_mode_t mode;
    int cpu_core;
    pthread_barrier_t *ready_barrier;
    pthread_barrier_t *start_barrier;
    pthread_barrier_t *done_barrier;
    atomic_int *launch;
    scan_stats_t stats;
    int validate;
    int stop;
    int failed;
} worker_arg_t;

typedef struct {
    worker_arg_t *args;
    pthread_t *threads;
    uint32_t count;
    pthread_barrier_t ready_barrier;
    pthread_barrier_t start_barrier;
    pthread_barrier_t done_barrier;
    atomic_int launch;
} worker_pool_t;

_Static_assert(sizeof(corpus_header_t) == 40, "corpus header layout changed");
_Static_assert(sizeof(corpus_record_header_t) == 16,
               "corpus record layout changed");
_Static_assert(sizeof(tuple_t) == 16, "tuple layout must be 16 bytes");

static uint64_t now_ns(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static uint64_t read_rss_kb(void)
{
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];
    uint64_t value = 0;

    if (fp == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "VmRSS: %" SCNu64 " kB", &value) == 1) {
            break;
        }
    }
    (void)fclose(fp);
    return value;
}

static uint64_t positive_delta(uint64_t before, uint64_t after)
{
    return after > before ? after - before : 0;
}

static int append_int(int **values, uint32_t *count, uint32_t *capacity,
                      int value)
{
    int *new_values;
    uint32_t new_capacity;

    if (*count == *capacity) {
        new_capacity = *capacity == 0 ? 16u : *capacity * 2u;
        new_values = (int *)realloc(*values, (size_t)new_capacity *
                                             sizeof(**values));
        if (new_values == NULL) {
            return -1;
        }
        *values = new_values;
        *capacity = new_capacity;
    }
    (*values)[(*count)++] = value;
    return 0;
}

static int parse_cpu_list(const char *text, int **cores_out,
                          uint32_t *count_out)
{
    char *copy;
    char *saveptr = NULL;
    char *part;
    int *cores = NULL;
    uint32_t count = 0;
    uint32_t capacity = 0;

    if (text == NULL || text[0] == '\0' || cores_out == NULL ||
        count_out == NULL) {
        return -1;
    }
    copy = strdup(text);
    if (copy == NULL) {
        return -1;
    }
    for (part = strtok_r(copy, ",", &saveptr);
         part != NULL;
         part = strtok_r(NULL, ",", &saveptr)) {
        char *dash = strchr(part, '-');
        char *endptr = NULL;
        long start;
        long end;

        if (dash != NULL) {
            *dash = '\0';
            start = strtol(part, &endptr, 10);
            if (*endptr != '\0') {
                goto error;
            }
            end = strtol(dash + 1, &endptr, 10);
            if (*endptr != '\0' || start < 0 || end < start ||
                end >= CPU_SETSIZE) {
                goto error;
            }
        } else {
            start = strtol(part, &endptr, 10);
            if (*endptr != '\0' || start < 0 || start >= CPU_SETSIZE) {
                goto error;
            }
            end = start;
        }
        for (long cpu = start; cpu <= end; cpu++) {
            if (append_int(&cores, &count, &capacity, (int)cpu) != 0) {
                goto error;
            }
        }
    }
    free(copy);
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1u; j < count; j++) {
            if (cores[i] == cores[j]) {
                free(cores);
                return -1;
            }
        }
    }
    *cores_out = cores;
    *count_out = count;
    return count == 0 ? -1 : 0;

error:
    free(copy);
    free(cores);
    return -1;
}

static int set_thread_affinity(int cpu_core)
{
    cpu_set_t cpuset;
    int ret;

    if (cpu_core < 0) {
        return 0;
    }
    if (cpu_core >= CPU_SETSIZE) {
        return -1;
    }
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);
    ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (ret != 0) {
        fprintf(stderr, "set affinity failed cpu=%d error=%d\n", cpu_core,
                ret);
        return -1;
    }
    return 0;
}

static const corpus_record_header_t *record_header(const corpus_t *corpus,
                                                    uint32_t index)
{
    return (const corpus_record_header_t *)(corpus->records +
                                             (size_t)index * corpus->record_size);
}

static const uint8_t *record_payload(const corpus_t *corpus, uint32_t index)
{
    return corpus->records + (size_t)index * corpus->record_size +
           sizeof(corpus_record_header_t);
}

static int load_corpus(const char *path, corpus_t *corpus)
{
    FILE *fp = fopen(path, "rb");
    uint64_t data_size;

    if (fp == NULL || fread(&corpus->header, sizeof(corpus->header), 1, fp) != 1 ||
        corpus->header.magic != CORPUS_MAGIC ||
        corpus->header.version != CORPUS_VERSION ||
        corpus->header.header_size != sizeof(corpus_header_t) ||
        corpus->header.record_header_size != sizeof(corpus_record_header_t) ||
        corpus->header.query_count == 0 || corpus->header.payload_size == 0) {
        fprintf(stderr, "invalid corpus: %s\n", path);
        if (fp != NULL) {
            (void)fclose(fp);
        }
        return -1;
    }
    corpus->record_size = sizeof(corpus_record_header_t) +
                          corpus->header.payload_size;
    data_size = (uint64_t)corpus->record_size * corpus->header.query_count;
    if (data_size > SIZE_MAX) {
        (void)fclose(fp);
        return -1;
    }
    corpus->records = (uint8_t *)malloc((size_t)data_size);
    if (corpus->records == NULL ||
        fread(corpus->records, (size_t)data_size, 1, fp) != 1 ||
        fgetc(fp) != EOF) {
        fprintf(stderr, "read corpus failed: %s\n", path);
        free(corpus->records);
        corpus->records = NULL;
        (void)fclose(fp);
        return -1;
    }
    (void)fclose(fp);
    return 0;
}

static void free_expression_set(expression_set_t *set)
{
    uint32_t i;
    for (i = 0; i < set->count; i++) {
        free(set->expressions[i]);
    }
    free(set->expressions);
    free(set->flags);
    free(set->ids);
    (void)memset(set, 0, sizeof(*set));
}

static int grow_expression_set(expression_set_t *set, uint32_t capacity)
{
    char **expressions = (char **)realloc(
        set->expressions, (size_t)capacity * sizeof(*set->expressions));
    uint32_t *flags;
    uint32_t *ids;

    if (expressions == NULL) {
        return -1;
    }
    set->expressions = expressions;
    flags = (uint32_t *)realloc(set->flags,
                               (size_t)capacity * sizeof(*set->flags));
    if (flags == NULL) {
        return -1;
    }
    set->flags = flags;
    ids = (uint32_t *)realloc(set->ids,
                             (size_t)capacity * sizeof(*set->ids));
    if (ids == NULL) {
        return -1;
    }
    set->ids = ids;
    return 0;
}

static int load_expressions(const char *path, expression_set_t *set)
{
    FILE *fp = fopen(path, "r");
    char *line = NULL;
    size_t line_capacity = 0;
    uint32_t capacity = 0;
    ssize_t length;

    if (fp == NULL) {
        fprintf(stderr, "open rules failed: %s errno=%d\n", path, errno);
        return -1;
    }
    while ((length = getline(&line, &line_capacity, fp)) >= 0) {
        char *tab1;
        char *tab2;
        char *expression;

        while (length > 0 &&
               (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        tab1 = strchr(line, '\t');
        tab2 = tab1 == NULL ? NULL : strchr(tab1 + 1, '\t');
        if (tab1 == NULL || tab2 == NULL || tab2[1] == '\0') {
            goto error;
        }
        *tab1 = '\0';
        *tab2 = '\0';
        expression = tab2 + 1;
        if (set->count == capacity) {
            capacity = capacity == 0 ? 1024u : capacity * 2u;
            if (grow_expression_set(set, capacity) != 0) {
                goto error;
            }
        }
        set->ids[set->count] = (uint32_t)strtoul(line, NULL, 10);
        set->flags[set->count] = (uint32_t)strtoul(tab1 + 1, NULL, 10);
        set->expressions[set->count] = strdup(expression);
        if (set->expressions[set->count] == NULL) {
            goto error;
        }
        if ((set->flags[set->count] & MS_FLAG_COMBINATION) != 0) {
            set->combo_count++;
        } else {
            set->atomic_count++;
        }
        set->count++;
    }
    free(line);
    (void)fclose(fp);
    return set->count == 0 ? -1 : 0;

error:
    fprintf(stderr, "invalid expression file: %s\n", path);
    free(line);
    (void)fclose(fp);
    free_expression_set(set);
    return -1;
}

static int load_hash_rules(const char *path, uint32_t count, tuple_t **rules)
{
    FILE *fp = fopen(path, "rb");

    *rules = (tuple_t *)malloc((size_t)count * sizeof(**rules));
    if (fp == NULL || *rules == NULL ||
        fread(*rules, sizeof(**rules), count, fp) != count) {
        fprintf(stderr, "read hash rules failed: %s\n", path);
        if (fp != NULL) {
            (void)fclose(fp);
        }
        free(*rules);
        *rules = NULL;
        return -1;
    }
    (void)fclose(fp);
    return 0;
}

static int add_hash_rules(ms_handler_t *handler, const tuple_t *rules,
                          uint32_t count)
{
    ms_rule_t *batch = (ms_rule_t *)malloc(HASH_ADD_BATCH * sizeof(*batch));
    uint32_t offset;

    if (batch == NULL) {
        return -1;
    }
    for (offset = 0; offset < count; offset += HASH_ADD_BATCH) {
        uint32_t batch_count = count - offset;
        uint32_t i;
        int ret;

        if (batch_count > HASH_ADD_BATCH) {
            batch_count = HASH_ADD_BATCH;
        }
        for (i = 0; i < batch_count; i++) {
            batch[i].alias = offset + i + 1u;
            batch[i].key_len = sizeof(tuple_t);
            batch[i].key = (uint8_t *)&rules[offset + i];
        }
        ret = ms_add_rule(handler, batch, batch_count);
        if (ret != MS_SUCCESS) {
            fprintf(stderr, "ms_add_rule failed offset=%u ret=%d\n", offset, ret);
            free(batch);
            return -1;
        }
    }
    free(batch);
    return 0;
}

static int add_legacy_ac_rules(ms_handler_t *handler, const expression_set_t *set)
{
    ms_rule_t *rules = (ms_rule_t *)calloc(set->count, sizeof(*rules));
    uint32_t i;
    int ret;

    if (rules == NULL) {
        return -1;
    }
    for (i = 0; i < set->count; i++) {
        rules[i].alias = set->ids[i];
        rules[i].key = (uint8_t *)set->expressions[i];
        rules[i].key_len = (uint32_t)strlen(set->expressions[i]);
    }
    ret = ms_add_rule(handler, rules, set->count);
    free(rules);
    if (ret != MS_SUCCESS) {
        fprintf(stderr, "legacy ms_add_rule failed ret=%d\n", ret);
        return -1;
    }
    return 0;
}

static int compile_database_atomic_only(ms_database_t **db,
                                        const expression_set_t *set)
{
    const char **expressions;
    uint32_t *flags;
    uint32_t *ids;
    uint32_t i;
    uint32_t atomic_index = 0;
    int ret;

    if (set->atomic_count == 0) {
        return -1;
    }

    expressions = (const char **)calloc((size_t)set->atomic_count + 1u,
                                         sizeof(*expressions));
    flags = (uint32_t *)calloc(set->atomic_count, sizeof(*flags));
    ids = (uint32_t *)calloc(set->atomic_count, sizeof(*ids));
    if (expressions == NULL || flags == NULL || ids == NULL) {
        free(expressions);
        free(flags);
        free(ids);
        return -1;
    }

    for (i = 0; i < set->count; i++) {
        if ((set->flags[i] & MS_FLAG_COMBINATION) != 0) {
            continue;
        }
        expressions[atomic_index] = set->expressions[i];
        flags[atomic_index] = set->flags[i] & ~MS_FLAG_QUIET;
        ids[atomic_index] = set->ids[i];
        atomic_index++;
    }

    ret = atomic_index == set->atomic_count ?
        ms_compile_database_multi(db, MS_MATCHER_TYPE_AC, 0, 0,
                                  (const char *const *)expressions, flags, ids,
                                  set->atomic_count) : -1;
    free(expressions);
    free(flags);
    free(ids);
    if (ret != MS_SUCCESS) {
        fprintf(stderr, "ms_compile_database_multi atomic-only failed\n");
        return -1;
    }
    return 0;
}

static int scan_result_ok(int ret)
{
    return ret == MS_SUCCESS || ret == MS_FIND_FAIL;
}

static int is_dynamic_delete_algorithm(algorithm_t algorithm)
{
    return algorithm == ALG_AC_DELETE || algorithm == ALG_ACCOMBO_DELETE;
}

static int output_contains_id(const uint32_t *ids, uint32_t count,
                              uint32_t id)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        if (ids[i] == id) {
            return 1;
        }
    }
    return 0;
}

static int find_expression_index(const expression_set_t *set, uint32_t id,
                                 int combination, uint32_t *index)
{
    uint32_t i;

    for (i = 0; i < set->count; i++) {
        int is_combination =
            (set->flags[i] & MS_FLAG_COMBINATION) != 0;
        if (set->ids[i] == id && is_combination == combination) {
            *index = i;
            return 0;
        }
    }
    return -1;
}

static int scan_one(scan_context_t *ctx, bench_mode_t mode,
                    const uint8_t *payload,
                    uint32_t payload_size, uint32_t *id_count_out)
{
    uint32_t count = 0;
    int ret;

    if (ctx->algorithm == ALG_HASH) {
        ms_rule_t rule = {0, sizeof(tuple_t), (uint8_t *)payload};
        ret = ms_find_rule(ctx->hash_handler, &rule);
        if (!scan_result_ok(ret)) {
            return -1;
        }
        *id_count_out = ret == MS_SUCCESS ? 1u : 0u;
        return 0;
    }
    if (ctx->algorithm == ALG_AC_LEGACY) {
        if (mode == MODE_BLOCK) {
            ret = ms_scan_expr(ctx->legacy_handler, payload, payload_size,
                               ctx->output_ids, &count, OUTPUT_CAPACITY);
        } else {
            if (ms_reset_stream(ctx->stream) != MS_SUCCESS) {
                return -1;
            }
            ret = ms_scan_expr_stream(ctx->stream, payload, payload_size,
                                      ctx->output_ids, &count, OUTPUT_CAPACITY);
        }
        if (!scan_result_ok(ret)) {
            return -1;
        }
        *id_count_out = count;
        return 0;
    }
    if (mode == MODE_BLOCK) {
        ret = ms_scan_database(ctx->db, payload, payload_size, ctx->scratch,
                               ctx->output_ids, &count, OUTPUT_CAPACITY);
        if (!scan_result_ok(ret)) {
            return -1;
        }
        *id_count_out = count;
        return 0;
    }

    if (ms_reset_stream(ctx->stream) != MS_SUCCESS) {
        return -1;
    }
    ret = ms_scan_stream(ctx->stream, payload, payload_size, ctx->scratch,
                         ctx->output_ids, &count, OUTPUT_CAPACITY);
    if (!scan_result_ok(ret)) {
        return -1;
    }
    *id_count_out = count;
    return 0;
}

static int scan_corpus(scan_context_t *ctx, bench_mode_t mode,
                       scan_stats_t *stats,
                       int validate)
{
    uint32_t i;

    (void)memset(stats, 0, sizeof(*stats));
    for (i = 0; i < ctx->corpus->header.query_count; i++) {
        const corpus_record_header_t *record = record_header(ctx->corpus, i);
        const uint8_t *payload = record_payload(ctx->corpus, i);
        uint32_t id_count = 0;

        if (scan_one(ctx, mode, payload, ctx->corpus->header.payload_size,
                     &id_count) != 0) {
            fprintf(stderr, "scan failed query=%u mode=%d\n", i, mode);
            return -1;
        }
        uint32_t expected_ids = ctx->expected_ids_after_delete != NULL ?
            ctx->expected_ids_after_delete[i] :
            (ctx->algorithm == ALG_AC_ON_COMBO ?
                 record->expected_candidates : record->expected_ids);

        if (validate && id_count != expected_ids) {
            fprintf(stderr,
                    "result mismatch query=%u mode=%d expected=%u actual=%u\n",
                    i, mode, expected_ids, id_count);
            return -1;
        }
        if (validate && expected_ids != 0 &&
            ctx->expected_output_ids_after_delete != NULL &&
            !output_contains_id(ctx->output_ids, id_count,
                                ctx->expected_output_ids_after_delete[i])) {
            fprintf(stderr,
                    "result id mismatch query=%u mode=%d expected_id=%u\n",
                    i, mode, ctx->expected_output_ids_after_delete[i]);
            return -1;
        }
        stats->queries++;
        stats->bytes += ctx->corpus->header.payload_size;
        stats->matches += id_count != 0;
        stats->expected_matches += expected_ids != 0;
        stats->reported_ids += id_count;
        stats->expected_ids += expected_ids;
        stats->expected_candidates += record->expected_candidates;
    }
    return 0;
}

static int build_dynamic_delete_plan(ms_database_t *db,
                                     algorithm_t algorithm,
                                     const expression_set_t *set,
                                     const corpus_t *corpus,
                                     dynamic_delete_plan_t *plan)
{
    ms_scratch_t *scratch = NULL;
    uint32_t output_ids[OUTPUT_CAPACITY];
    uint32_t target_index = UINT32_MAX;
    uint32_t affected_count = 0;
    uint32_t i;
    int ret = -1;

    plan->expected_ids = (uint32_t *)calloc(
        corpus->header.query_count, sizeof(*plan->expected_ids));
    plan->expected_output_ids = (uint32_t *)calloc(
        corpus->header.query_count, sizeof(*plan->expected_output_ids));
    if (plan->expected_ids == NULL || plan->expected_output_ids == NULL ||
        ms_alloc_scratch(db, &scratch) != MS_SUCCESS) {
        goto end;
    }

    for (i = 0; i < corpus->header.query_count; i++) {
        const corpus_record_header_t *record = record_header(corpus, i);
        uint32_t count = 0;
        int scan_ret = ms_scan_database(
            db, record_payload(corpus, i), corpus->header.payload_size,
            scratch, output_ids, &count, OUTPUT_CAPACITY);

        if (!scan_result_ok(scan_ret) || count != record->expected_ids) {
            fprintf(stderr,
                    "dynamic-delete baseline mismatch query=%u expected=%u actual=%u ret=%d\n",
                    i, record->expected_ids, count, scan_ret);
            goto end;
        }
        if (count != 0) {
            plan->affected_id = output_ids[0];
            plan->probe_query = i;
            break;
        }
    }
    if (i == corpus->header.query_count) {
        fprintf(stderr, "dynamic-delete corpus has no matching query\n");
        goto end;
    }

    if (algorithm == ALG_AC_DELETE) {
        if (find_expression_index(set, plan->affected_id, 0,
                                  &target_index) != 0) {
            goto end;
        }
    } else {
        uint32_t combo_index;
        char *end = NULL;
        unsigned long dependency_id;

        if (find_expression_index(set, plan->affected_id, 1,
                                  &combo_index) != 0) {
            goto end;
        }
        dependency_id = strtoul(set->expressions[combo_index], &end, 10);
        if (end == set->expressions[combo_index] ||
            (*end != '&' && *end != '|') || dependency_id > UINT32_MAX ||
            find_expression_index(set, (uint32_t)dependency_id, 0,
                                  &target_index) != 0) {
            fprintf(stderr, "unsupported generated combination: %s\n",
                    set->expressions[combo_index]);
            goto end;
        }
    }

    plan->rule.alias = set->ids[target_index];
    plan->rule.key = (uint8_t *)set->expressions[target_index];
    plan->rule.key_len = (uint32_t)strlen(set->expressions[target_index]);

    for (i = 0; i < corpus->header.query_count; i++) {
        const corpus_record_header_t *record = record_header(corpus, i);
        uint32_t count = 0;
        int scan_ret = ms_scan_database(
            db, record_payload(corpus, i), corpus->header.payload_size,
            scratch, output_ids, &count, OUTPUT_CAPACITY);

        if (!scan_result_ok(scan_ret) || count != record->expected_ids) {
            goto end;
        }
        plan->expected_ids[i] = count;
        if (count != 0) {
            plan->expected_output_ids[i] = output_ids[0];
        }
        if (output_contains_id(output_ids, count, plan->affected_id)) {
            uint32_t j;

            plan->expected_ids[i]--;
            plan->expected_output_ids[i] = 0;
            for (j = 0; j < count; j++) {
                if (output_ids[j] != plan->affected_id) {
                    plan->expected_output_ids[i] = output_ids[j];
                    break;
                }
            }
            affected_count++;
        }
    }
    if (affected_count == 0) {
        fprintf(stderr, "dynamic-delete target id=%u has no affected queries\n",
                plan->affected_id);
        goto end;
    }
    ret = 0;

end:
    if (scratch != NULL) {
        (void)ms_free_scratch(scratch);
    }
    if (ret != 0) {
        free(plan->expected_ids);
        free(plan->expected_output_ids);
        (void)memset(plan, 0, sizeof(*plan));
    }
    return ret;
}

static int apply_dynamic_delete(ms_database_t *db,
                                worker_pool_t *pool,
                                const corpus_t *corpus,
                                dynamic_delete_plan_t *plan,
                                uint64_t *delete_ns)
{
    ms_rule_t failed_batch[2];
    uint32_t i;
    uint64_t start;
    int ret;

    failed_batch[0] = plan->rule;
    failed_batch[1] = plan->rule;
    failed_batch[1].alias = UINT32_MAX;
    ret = ms_remove_database_rule(db, failed_batch, 2);
    if (ret != MS_FIND_FAIL) {
        fprintf(stderr,
                "dynamic-delete atomic failure check ret=%d expected=%d\n",
                ret, MS_FIND_FAIL);
        return -1;
    }

    {
        scan_context_t *ctx = &pool->args[0].context;
        uint32_t count = 0;

        ret = ms_scan_database(db,
                               record_payload(corpus, plan->probe_query),
                               corpus->header.payload_size, ctx->scratch,
                               ctx->output_ids, &count, OUTPUT_CAPACITY);
        if (!scan_result_ok(ret) ||
            !output_contains_id(ctx->output_ids, count,
                                plan->affected_id)) {
            fprintf(stderr,
                    "dynamic-delete failed batch changed live database ret=%d\n",
                    ret);
            return -1;
        }
    }

    start = now_ns();
    ret = ms_remove_database_rule(db, &plan->rule, 1);
    *delete_ns = now_ns() - start;
    if (ret != MS_SUCCESS) {
        fprintf(stderr, "dynamic-delete remove failed id=%u ret=%d\n",
                plan->rule.alias, ret);
        return -1;
    }

    for (i = 0; i < pool->count; i++) {
        scan_context_t *ctx = &pool->args[i].context;
        uint32_t count = 0;

        ctx->expected_ids_after_delete = plan->expected_ids;
        ctx->expected_output_ids_after_delete = plan->expected_output_ids;
        if (ctx->stream == NULL) {
            continue;
        }
        ret = ms_scan_stream(ctx->stream,
                             record_payload(corpus, plan->probe_query),
                             corpus->header.payload_size, ctx->scratch,
                             ctx->output_ids, &count, OUTPUT_CAPACITY);
        if (ret != MS_STREAM_STALE) {
            fprintf(stderr,
                    "dynamic-delete stale check worker=%u ret=%d expected=%d\n",
                    i, ret, MS_STREAM_STALE);
            return -1;
        }
        if (ms_reset_stream(ctx->stream) != MS_SUCCESS) {
            fprintf(stderr, "dynamic-delete reset failed worker=%u\n", i);
            return -1;
        }
    }
    fprintf(stderr,
            "[phase] dynamic delete done removed_id=%u affected_id=%u delete_ns=%" PRIu64 "\n",
            plan->rule.alias, plan->affected_id, *delete_ns);
    return 0;
}

static int init_worker_context(scan_context_t *context,
                               const scan_context_t *shared,
                               bench_mode_t selected_mode)
{
    *context = *shared;
    context->scratch = NULL;
    context->stream = NULL;
    (void)memset(context->output_ids, 0, sizeof(context->output_ids));

    if (context->algorithm == ALG_HASH) {
        return 0;
    }
    if (context->algorithm == ALG_AC_LEGACY) {
        if ((selected_mode == MODE_STREAM || selected_mode == MODE_BOTH) &&
            ms_open_handler_stream(context->legacy_handler,
                                   &context->stream) != MS_SUCCESS) {
            return -1;
        }
        return 0;
    }
    if (ms_alloc_scratch(context->db, &context->scratch) != MS_SUCCESS) {
        return -1;
    }
    if ((selected_mode == MODE_STREAM || selected_mode == MODE_BOTH) &&
        ms_open_stream(context->db, &context->stream) != MS_SUCCESS) {
        (void)ms_free_scratch(context->scratch);
        context->scratch = NULL;
        return -1;
    }
    return 0;
}

static void destroy_worker_context(scan_context_t *context)
{
    uint32_t output_count = 0;

    if (context->stream != NULL) {
        if (context->algorithm == ALG_AC_LEGACY) {
            (void)ms_close_handler_stream(context->stream);
        } else {
            (void)ms_reset_stream(context->stream);
            (void)ms_close_stream(context->stream, context->scratch,
                                  context->output_ids, &output_count,
                                  OUTPUT_CAPACITY);
        }
        context->stream = NULL;
    }
    if (context->scratch != NULL) {
        (void)ms_free_scratch(context->scratch);
        context->scratch = NULL;
    }
}

static void *worker_main(void *opaque)
{
    worker_arg_t *arg = (worker_arg_t *)opaque;
    int launch;

    do {
        launch = atomic_load_explicit(arg->launch, memory_order_acquire);
        if (launch == 0) {
            sched_yield();
        }
    } while (launch == 0);
    if (launch < 0) {
        return NULL;
    }
    if (set_thread_affinity(arg->cpu_core) != 0) {
        arg->failed = 1;
    }
    (void)pthread_barrier_wait(arg->ready_barrier);

    for (;;) {
        (void)pthread_barrier_wait(arg->start_barrier);
        if (arg->stop) {
            break;
        }
        arg->failed = scan_corpus(&arg->context, arg->mode, &arg->stats,
                                  arg->validate) != 0;
        (void)pthread_barrier_wait(arg->done_barrier);
    }
    return NULL;
}

static void destroy_worker_pool(worker_pool_t *pool)
{
    uint32_t i;

    if (pool->args == NULL) {
        return;
    }
    for (i = 0; i < pool->count; i++) {
        pool->args[i].stop = 1;
    }
    (void)pthread_barrier_wait(&pool->start_barrier);
    for (i = 0; i < pool->count; i++) {
        (void)pthread_join(pool->threads[i], NULL);
        destroy_worker_context(&pool->args[i].context);
    }
    (void)pthread_barrier_destroy(&pool->ready_barrier);
    (void)pthread_barrier_destroy(&pool->start_barrier);
    (void)pthread_barrier_destroy(&pool->done_barrier);
    free(pool->threads);
    free(pool->args);
    (void)memset(pool, 0, sizeof(*pool));
}

static int init_worker_pool(worker_pool_t *pool,
                            const scan_context_t *shared,
                            bench_mode_t selected_mode,
                            const int *cpu_cores,
                            uint32_t worker_count)
{
    uint32_t initialized = 0;
    uint32_t created = 0;
    uint32_t i;

    atomic_init(&pool->launch, 0);
    pool->count = worker_count;
    pool->args = (worker_arg_t *)calloc(worker_count, sizeof(*pool->args));
    pool->threads = (pthread_t *)calloc(worker_count, sizeof(*pool->threads));
    if (pool->args == NULL || pool->threads == NULL) {
        goto error;
    }
    if (pthread_barrier_init(&pool->ready_barrier, NULL,
                             worker_count + 1u) != 0) {
        goto error;
    }
    initialized = 1;
    if (pthread_barrier_init(&pool->start_barrier, NULL,
                             worker_count + 1u) != 0) {
        goto error;
    }
    initialized = 2;
    if (pthread_barrier_init(&pool->done_barrier, NULL,
                             worker_count + 1u) != 0) {
        goto error;
    }
    initialized = 3;
    for (i = 0; i < worker_count; i++) {
        worker_arg_t *arg = &pool->args[i];

        if (init_worker_context(&arg->context, shared, selected_mode) != 0) {
            goto error;
        }
        arg->cpu_core = cpu_cores == NULL ? -1 : cpu_cores[i];
        arg->ready_barrier = &pool->ready_barrier;
        arg->start_barrier = &pool->start_barrier;
        arg->done_barrier = &pool->done_barrier;
        arg->launch = &pool->launch;
        if (pthread_create(&pool->threads[i], NULL, worker_main, arg) != 0) {
            destroy_worker_context(&arg->context);
            goto error;
        }
        created++;
    }
    atomic_store_explicit(&pool->launch, 1, memory_order_release);
    (void)pthread_barrier_wait(&pool->ready_barrier);
    for (i = 0; i < worker_count; i++) {
        if (pool->args[i].failed) {
            destroy_worker_pool(pool);
            return -1;
        }
    }
    return 0;

error:
    atomic_store_explicit(&pool->launch, -1, memory_order_release);
    if (pool->args != NULL) {
        for (i = 0; i < created; i++) {
            (void)pthread_join(pool->threads[i], NULL);
            destroy_worker_context(&pool->args[i].context);
        }
        for (i = created; i < worker_count; i++) {
            destroy_worker_context(&pool->args[i].context);
        }
    }
    while (initialized != 0) {
        if (initialized == 3u) {
            (void)pthread_barrier_destroy(&pool->done_barrier);
        } else if (initialized == 2u) {
            (void)pthread_barrier_destroy(&pool->start_barrier);
        } else {
            (void)pthread_barrier_destroy(&pool->ready_barrier);
        }
        initialized--;
    }
    free(pool->threads);
    free(pool->args);
    (void)memset(pool, 0, sizeof(*pool));
    return -1;
}

static int run_worker_pool(worker_pool_t *pool, bench_mode_t mode,
                           int validate, scan_stats_t *stats,
                           uint64_t *elapsed_ns)
{
    uint64_t start;
    uint32_t i;

    (void)memset(stats, 0, sizeof(*stats));
    for (i = 0; i < pool->count; i++) {
        pool->args[i].mode = mode;
        pool->args[i].validate = validate;
        pool->args[i].failed = 0;
    }
    start = now_ns();
    (void)pthread_barrier_wait(&pool->start_barrier);
    (void)pthread_barrier_wait(&pool->done_barrier);
    *elapsed_ns = now_ns() - start;

    for (i = 0; i < pool->count; i++) {
        const scan_stats_t *worker = &pool->args[i].stats;

        if (pool->args[i].failed) {
            return -1;
        }
        stats->queries += worker->queries;
        stats->bytes += worker->bytes;
        stats->matches += worker->matches;
        stats->expected_matches += worker->expected_matches;
        stats->reported_ids += worker->reported_ids;
        stats->expected_ids += worker->expected_ids;
        stats->expected_candidates += worker->expected_candidates;
    }
    return 0;
}

static const char *algorithm_name(algorithm_t algorithm)
{
    return algorithm == ALG_HASH ? "hash" :
           algorithm == ALG_AC ? "ac" :
           algorithm == ALG_ACCOMBO ? "accombo" :
           algorithm == ALG_AC_LEGACY ? "ac_legacy" :
           algorithm == ALG_AC_ON_COMBO ? "ac_on_combo" :
           algorithm == ALG_AC_DELETE ? "ac_delete" : "accombo_delete";
}

static const char *mode_name(bench_mode_t mode)
{
    return mode == MODE_LOOKUP ? "lookup" :
           mode == MODE_BLOCK ? "block" : "stream-full";
}

static int append_result(const char *csv_path, const char *machine,
                         algorithm_t algorithm, bench_mode_t mode,
                         uint32_t trial,
                         uint32_t rule_count, const expression_set_t *set,
                         const corpus_t *corpus, const scan_stats_t *stats,
                         uint64_t elapsed_ns, uint64_t compile_ns,
                         uint64_t rss_growth_kb, uint32_t warmups,
                         uint32_t worker_count, const char *cpu_list)
{
    FILE *fp = fopen(csv_path, "a");
    double seconds = (double)elapsed_ns / 1e9;
    double query_ops = (double)stats->queries / seconds;
    double gib_s = ((double)stats->bytes / 1073741824.0) / seconds;
    double hit_rate = (double)corpus->header.hit_count /
                      corpus->header.query_count;
    uint64_t expected_hits = stats->expected_matches;
    uint32_t atomic_count = algorithm == ALG_HASH ? rule_count : set->atomic_count;
    uint32_t combo_count =
        (algorithm == ALG_ACCOMBO || algorithm == ALG_ACCOMBO_DELETE) ?
            set->combo_count : 0u;
    const char *timing_scope = is_dynamic_delete_algorithm(algorithm) ?
        "scan_after_preload_compile_worker_setup_delete_warmup" :
        "scan_only_after_preload_compile_worker_setup_warmup";

    if (fp == NULL) {
        return -1;
    }
    fprintf(fp,
            "%s,%s,%s,%u,%u,%u,%u,%" PRIu64 ",%u,%.8f,%" PRIu64 ",%" PRIu64
            ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
            ",%" PRIu64 ",%.2f,%.8f,%" PRIu64 ",%" PRIu64
            ",%u,%s,%u,\"%s\"\n",
            machine, algorithm_name(algorithm), mode_name(mode), trial,
            rule_count, atomic_count, combo_count, stats->queries,
            corpus->header.payload_size, hit_rate, expected_hits,
            stats->matches, stats->expected_ids, stats->reported_ids,
            stats->expected_candidates, stats->bytes, elapsed_ns, query_ops,
            gib_s, compile_ns, rss_growth_kb, warmups, timing_scope,
            worker_count, cpu_list);
    return fclose(fp) == 0 ? 0 : -1;
}

static int timed_mode(worker_pool_t *pool, const scan_context_t *shared,
                      bench_mode_t mode, uint32_t trial,
                      const char *csv_path, const char *machine,
                      uint32_t rule_count, const expression_set_t *set,
                      uint64_t compile_ns, uint64_t rss_growth_kb,
                      uint32_t warmups, const char *cpu_list)
{
    scan_stats_t stats;
    uint64_t elapsed;

    if (run_worker_pool(pool, mode, 0, &stats, &elapsed) != 0) {
        return -1;
    }
    printf("machine=%s algorithm=%s mode=%s trial=%u workers=%u query_ops=%.2f "
           "throughput_gib_s=%.8f matches=%" PRIu64 " ids=%" PRIu64 "\n",
           machine, algorithm_name(shared->algorithm), mode_name(mode), trial,
           pool->count,
           (double)stats.queries / ((double)elapsed / 1e9),
           ((double)stats.bytes / 1073741824.0) / ((double)elapsed / 1e9),
           stats.matches, stats.reported_ids);
    return append_result(csv_path, machine, shared->algorithm, mode, trial,
                         rule_count, set, shared->corpus, &stats, elapsed,
                         compile_ns, rss_growth_kb, warmups, pool->count,
                         cpu_list);
}

static int parse_algorithm(const char *text, algorithm_t *algorithm)
{
    if (strcmp(text, "hash") == 0) {
        *algorithm = ALG_HASH;
    } else if (strcmp(text, "ac") == 0) {
        *algorithm = ALG_AC;
    } else if (strcmp(text, "accombo") == 0) {
        *algorithm = ALG_ACCOMBO;
    } else if (strcmp(text, "ac_legacy") == 0) {
        *algorithm = ALG_AC_LEGACY;
    } else if (strcmp(text, "ac_on_combo") == 0) {
        *algorithm = ALG_AC_ON_COMBO;
    } else if (strcmp(text, "ac_delete") == 0) {
        *algorithm = ALG_AC_DELETE;
    } else if (strcmp(text, "accombo_delete") == 0) {
        *algorithm = ALG_ACCOMBO_DELETE;
    } else {
        return -1;
    }
    return 0;
}

static int parse_mode(const char *text, bench_mode_t *mode)
{
    if (strcmp(text, "lookup") == 0) {
        *mode = MODE_LOOKUP;
    } else if (strcmp(text, "block") == 0) {
        *mode = MODE_BLOCK;
    } else if (strcmp(text, "stream-full") == 0) {
        *mode = MODE_STREAM;
    } else if (strcmp(text, "both") == 0) {
        *mode = MODE_BOTH;
    } else {
        return -1;
    }
    return 0;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s -a hash|ac|accombo|ac_legacy|ac_on_combo|"
            "ac_delete|accombo_delete "
            "-M lookup|block|stream-full|both "
            "-n rules -t rules_file -s corpus -p csv -T machine "
            "[-R trials] [-w warmups] [-c cpu-list]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *algorithm_text = NULL;
    const char *mode_text = NULL;
    const char *rules_path = NULL;
    const char *corpus_path = NULL;
    const char *csv_path = NULL;
    const char *machine = NULL;
    const char *cpu_list = NULL;
    const char *cpu_list_label = "inherited";
    algorithm_t algorithm = ALG_AC;
    bench_mode_t selected_mode = MODE_BOTH;
    uint32_t rule_count = 0;
    uint32_t trials = 5;
    uint32_t warmups = 1;
    tuple_t *hash_rules = NULL;
    expression_set_t expressions = {0};
    corpus_t corpus = {0};
    ms_handler_t hash_handler = {0};
    ms_handler_t legacy_handler = {0};
    int hash_inited = 0;
    int legacy_inited = 0;
    ms_database_t *db = NULL;
    scan_context_t shared_context = {0};
    worker_pool_t pool = {0};
    dynamic_delete_plan_t delete_plan = {0};
    int *cpu_cores = NULL;
    uint32_t worker_count = 1;
    scan_stats_t ignored;
    uint64_t ignored_elapsed;
    uint64_t before_rss;
    uint64_t compile_start;
    uint64_t compile_ns;
    uint64_t rss_growth;
    uint64_t delete_ns = 0;
    uint32_t i;
    int argi;
    int result = EXIT_FAILURE;

    for (argi = 1; argi < argc; argi += 2) {
        const char *option = argv[argi];
        const char *value;

        if (argi + 1 >= argc || option[0] != '-' || option[2] != '\0') {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        value = argv[argi + 1];
        if (strcmp(option, "-a") == 0) {
            algorithm_text = value;
        } else if (strcmp(option, "-M") == 0) {
            mode_text = value;
        } else if (strcmp(option, "-n") == 0) {
            rule_count = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-t") == 0) {
            rules_path = value;
        } else if (strcmp(option, "-s") == 0) {
            corpus_path = value;
        } else if (strcmp(option, "-p") == 0) {
            csv_path = value;
        } else if (strcmp(option, "-T") == 0) {
            machine = value;
        } else if (strcmp(option, "-R") == 0) {
            trials = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-w") == 0) {
            warmups = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-c") == 0) {
            cpu_list = value;
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (algorithm_text == NULL || mode_text == NULL || rules_path == NULL ||
        corpus_path == NULL || csv_path == NULL || machine == NULL ||
        rule_count == 0 || trials == 0u ||
        parse_algorithm(algorithm_text, &algorithm) != 0 ||
        parse_mode(mode_text, &selected_mode) != 0 ||
        (algorithm == ALG_HASH && selected_mode != MODE_LOOKUP) ||
        (algorithm != ALG_HASH && selected_mode == MODE_LOOKUP)) {
        usage(argv[0]);
        goto end;
    }
    if (cpu_list != NULL) {
        if (parse_cpu_list(cpu_list, &cpu_cores, &worker_count) != 0) {
            fprintf(stderr, "invalid cpu list: %s\n", cpu_list);
            goto end;
        }
        cpu_list_label = cpu_list;
    }
    fprintf(stderr, "[phase] preload corpus start\n");
    if (load_corpus(corpus_path, &corpus) != 0 ||
        (algorithm == ALG_AC_LEGACY || algorithm == ALG_AC_DELETE ?
             corpus.header.kind != KIND_AC :
         algorithm == ALG_AC_ON_COMBO || algorithm == ALG_ACCOMBO_DELETE ?
             corpus.header.kind != KIND_ACCOMBO :
             corpus.header.kind != (uint32_t)algorithm)) {
        goto end;
    }
    if ((algorithm == ALG_HASH && corpus.header.payload_size != sizeof(tuple_t)) ||
        (algorithm != ALG_HASH && corpus.header.payload_size < 64u)) {
        goto end;
    }
    if (algorithm == ALG_HASH) {
        if (load_hash_rules(rules_path, rule_count, &hash_rules) != 0) {
            goto end;
        }
    } else if (load_expressions(rules_path, &expressions) != 0 ||
               expressions.count != rule_count) {
        goto end;
    }
    fprintf(stderr, "[phase] preload corpus done queries=%u payload=%u\n",
            corpus.header.query_count, corpus.header.payload_size);

    before_rss = read_rss_kb();
    compile_start = now_ns();
    fprintf(stderr, "[phase] compile start algorithm=%s rules=%u\n",
            algorithm_name(algorithm), rule_count);
    if (algorithm == ALG_HASH) {
        if (ms_handler_init(&hash_handler, MS_MATCHER_TYPE_HASH, 0, 0) !=
            MS_SUCCESS) {
            goto end;
        }
        hash_inited = 1;
        if (add_hash_rules(&hash_handler, hash_rules, rule_count) != 0) {
            goto end;
        }
    } else if (algorithm == ALG_AC_LEGACY) {
        if (ms_handler_init(&legacy_handler, MS_MATCHER_TYPE_AC, 0, 0) !=
            MS_SUCCESS) {
            goto end;
        }
        legacy_inited = 1;
        if (add_legacy_ac_rules(&legacy_handler, &expressions) != 0) {
            goto end;
        }
    } else if (algorithm == ALG_AC_ON_COMBO) {
        if (compile_database_atomic_only(&db, &expressions) != 0) {
            goto end;
        }
    } else if (ms_compile_database_multi(
                   &db, MS_MATCHER_TYPE_AC, 0, 0,
                   (const char *const *)expressions.expressions,
                   expressions.flags, expressions.ids,
                   expressions.count) != MS_SUCCESS) {
        fprintf(stderr, "ms_compile_database_multi failed\n");
        goto end;
    }
    compile_ns = now_ns() - compile_start;
    rss_growth = positive_delta(before_rss, read_rss_kb());
    fprintf(stderr, "[phase] compile done compile_ns=%" PRIu64 " rss_kb=%" PRIu64 "\n",
            compile_ns, rss_growth);

    shared_context.algorithm = algorithm;
    shared_context.corpus = &corpus;
    shared_context.hash_handler = &hash_handler;
    shared_context.legacy_handler = &legacy_handler;
    shared_context.db = db;
    if (init_worker_pool(&pool, &shared_context, selected_mode, cpu_cores,
                         worker_count) != 0) {
        fprintf(stderr, "worker setup failed workers=%u cpu_list=%s\n",
                worker_count, cpu_list_label);
        goto end;
    }

    fprintf(stderr,
            "[phase] correctness and warmup start workers=%u cpu_list=%s\n",
            worker_count, cpu_list_label);
    if (algorithm == ALG_HASH) {
        if (run_worker_pool(&pool, MODE_LOOKUP, 1, &ignored,
                            &ignored_elapsed) != 0) {
            goto end;
        }
        for (i = 0; i < warmups; i++) {
            if (run_worker_pool(&pool, MODE_LOOKUP, 0, &ignored,
                                &ignored_elapsed) != 0) {
                goto end;
            }
        }
    } else {
        if ((selected_mode == MODE_BLOCK || selected_mode == MODE_BOTH) &&
            run_worker_pool(&pool, MODE_BLOCK, 1, &ignored,
                            &ignored_elapsed) != 0) {
            goto end;
        }
        if ((selected_mode == MODE_STREAM || selected_mode == MODE_BOTH) &&
            run_worker_pool(&pool, MODE_STREAM, 1, &ignored,
                            &ignored_elapsed) != 0) {
            goto end;
        }
        if (is_dynamic_delete_algorithm(algorithm)) {
            /* Workers are idle at start_barrier while the shared DB changes. */
            if (build_dynamic_delete_plan(db, algorithm, &expressions,
                                          &corpus, &delete_plan) != 0 ||
                apply_dynamic_delete(db, &pool, &corpus, &delete_plan,
                                     &delete_ns) != 0) {
                goto end;
            }
            shared_context.expected_ids_after_delete =
                delete_plan.expected_ids;
            if ((selected_mode == MODE_BLOCK || selected_mode == MODE_BOTH) &&
                run_worker_pool(&pool, MODE_BLOCK, 1, &ignored,
                                &ignored_elapsed) != 0) {
                goto end;
            }
            if ((selected_mode == MODE_STREAM || selected_mode == MODE_BOTH) &&
                run_worker_pool(&pool, MODE_STREAM, 1, &ignored,
                                &ignored_elapsed) != 0) {
                goto end;
            }
        }
        for (i = 0; i < warmups; i++) {
            if ((selected_mode == MODE_BLOCK || selected_mode == MODE_BOTH) &&
                run_worker_pool(&pool, MODE_BLOCK, 0, &ignored,
                                &ignored_elapsed) != 0) {
                goto end;
            }
            if ((selected_mode == MODE_STREAM || selected_mode == MODE_BOTH) &&
                run_worker_pool(&pool, MODE_STREAM, 0, &ignored,
                                &ignored_elapsed) != 0) {
                goto end;
            }
        }
    }
    if (is_dynamic_delete_algorithm(algorithm)) {
        fprintf(stderr,
                "[phase] dynamic-delete correctness done delete_ns=%" PRIu64 "\n",
                delete_ns);
    }
    fprintf(stderr, "[phase] timed trials start trials=%u\n", trials);
    wait_for_attached_profiler();
    for (i = 1; i <= trials; i++) {
        if (algorithm == ALG_HASH) {
            if (timed_mode(&pool, &shared_context, MODE_LOOKUP, i, csv_path,
                           machine,
                           rule_count, &expressions, compile_ns, rss_growth,
                           warmups, cpu_list_label) != 0) {
                goto end;
            }
        } else if (selected_mode == MODE_BOTH) {
            bench_mode_t first = (i & 1u) != 0 ? MODE_BLOCK : MODE_STREAM;
            bench_mode_t second = first == MODE_BLOCK ? MODE_STREAM : MODE_BLOCK;
            if (timed_mode(&pool, &shared_context, first, i, csv_path, machine,
                           rule_count, &expressions, compile_ns, rss_growth,
                           warmups, cpu_list_label) != 0 ||
                timed_mode(&pool, &shared_context, second, i, csv_path, machine,
                           rule_count, &expressions, compile_ns, rss_growth,
                           warmups, cpu_list_label) != 0) {
                goto end;
            }
        } else if (timed_mode(&pool, &shared_context, selected_mode, i,
                              csv_path, machine, rule_count, &expressions,
                              compile_ns, rss_growth, warmups,
                              cpu_list_label) != 0) {
            goto end;
        }
    }
    result = EXIT_SUCCESS;

end:
    destroy_worker_pool(&pool);
    free(delete_plan.expected_ids);
    free(delete_plan.expected_output_ids);
    if (db != NULL) {
        (void)ms_free_database(db);
    }
    if (hash_inited) {
        (void)ms_handler_destroy(&hash_handler);
    }
    if (legacy_inited) {
        (void)ms_handler_destroy(&legacy_handler);
    }
    free(hash_rules);
    free(cpu_cores);
    free_expression_set(&expressions);
    free(corpus.records);
    return result;
}

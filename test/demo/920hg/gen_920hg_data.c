/* Deterministic controlled corpora for the 920/HG Megascan benchmark. */
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CORPUS_MAGIC 0x4d534243u
#define CORPUS_VERSION 1u
#define KIND_HASH 1u
#define KIND_AC 2u
#define KIND_ACCOMBO 3u
#define LITERAL_SIZE 16u

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

_Static_assert(sizeof(corpus_header_t) == 40, "corpus header layout changed");
_Static_assert(sizeof(corpus_record_header_t) == 16,
               "corpus record layout changed");
_Static_assert(sizeof(tuple_t) == 16, "tuple layout must be 16 bytes");

static uint64_t mix64(uint64_t value)
{
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

static uint64_t rng_next(uint64_t *state)
{
    *state = mix64(*state);
    return *state;
}

static void fill_tuple(tuple_t *tuple, uint32_t index, uint32_t seed,
                       int is_miss)
{
    uint64_t mixed = mix64(((uint64_t)seed << 32) | index);

    (void)memset(tuple, 0, sizeof(*tuple));
    tuple->src_ip = index;
    tuple->src_port = (uint16_t)(mixed >> 48);
    tuple->protocol = is_miss ? 17u : 6u;
    tuple->dst_ip = (uint32_t)mixed;
    tuple->dst_port = (uint16_t)(mixed >> 32);
}

static void make_literal(uint32_t index, uint32_t seed,
                         char literal[LITERAL_SIZE + 1u])
{
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // pragma: allowlist secret
    uint64_t mixed = mix64(((uint64_t)seed << 32) | index);
    uint32_t value = index;
    uint32_t i;

    literal[0] = 'K';
    for (i = 1; i < 8; i++) {
        literal[i] = alphabet[value & 31u];
        value >>= 5;
    }
    for (; i < LITERAL_SIZE; i++) {
        literal[i] = alphabet[mixed & 31u];
        mixed >>= 5;
    }
    literal[LITERAL_SIZE] = '\0';
}

static uint32_t *make_query_order(uint32_t count, uint32_t seed)
{
    uint32_t *order = (uint32_t *)malloc((size_t)count * sizeof(*order));
    uint64_t state = ((uint64_t)seed << 1) | 1u;
    uint32_t i;

    if (order == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        order[i] = i;
    }
    for (i = count; i > 1; i--) {
        uint32_t j = (uint32_t)(rng_next(&state) % i);
        uint32_t tmp = order[i - 1u];
        order[i - 1u] = order[j];
        order[j] = tmp;
    }
    return order;
}

static int write_hash_rules(const char *path, uint32_t rule_count,
                            uint32_t seed)
{
    FILE *fp = fopen(path, "wb");
    uint32_t i;

    if (fp == NULL) {
        fprintf(stderr, "open failed: %s errno=%d\n", path, errno);
        return -1;
    }
    for (i = 0; i < rule_count; i++) {
        tuple_t tuple;
        fill_tuple(&tuple, i, seed, 0);
        if (fwrite(&tuple, sizeof(tuple), 1, fp) != 1) {
            fprintf(stderr, "write failed: %s\n", path);
            (void)fclose(fp);
            return -1;
        }
    }
    return fclose(fp) == 0 ? 0 : -1;
}

static int write_ac_rules(const char *path, uint32_t rule_count,
                          uint32_t seed)
{
    FILE *fp = fopen(path, "w");
    uint32_t i;

    if (fp == NULL) {
        fprintf(stderr, "open failed: %s errno=%d\n", path, errno);
        return -1;
    }
    for (i = 0; i < rule_count; i++) {
        char literal[LITERAL_SIZE + 1u];
        make_literal(i, seed, literal);
        if (fprintf(fp, "%u\t0\t%s\n", i + 1u, literal) < 0) {
            (void)fclose(fp);
            return -1;
        }
    }
    return fclose(fp) == 0 ? 0 : -1;
}

static int write_combo_rules(const char *path, uint32_t expression_count,
                             uint32_t combo_count, uint32_t seed)
{
    uint32_t atomic_count = expression_count - combo_count;
    FILE *fp;
    uint32_t i;

    if (combo_count < 2u || combo_count * 2u > atomic_count) {
        fprintf(stderr,
                "combo_count must be >=2 and 2*combo_count <= atomic_count\n");
        return -1;
    }
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "open failed: %s errno=%d\n", path, errno);
        return -1;
    }
    for (i = 0; i < atomic_count; i++) {
        char literal[LITERAL_SIZE + 1u];
        make_literal(i, seed, literal);
        if (fprintf(fp, "%u\t2\t%s\n", i + 1u, literal) < 0) {
            (void)fclose(fp);
            return -1;
        }
    }
    for (i = 0; i < combo_count; i++) {
        uint32_t left = i * 2u + 1u;
        uint32_t right = left + 1u;
        uint32_t id = atomic_count + i + 1u;
        if (fprintf(fp, "%u\t1\t%u&%u\n", id, left, right) < 0) {
            (void)fclose(fp);
            return -1;
        }
    }
    return fclose(fp) == 0 ? 0 : -1;
}

static void fill_filler(uint8_t *payload, uint32_t payload_size,
                        uint32_t query_index, uint32_t seed)
{
    uint64_t state = mix64(((uint64_t)seed << 32) | query_index);
    uint32_t i;

    for (i = 0; i < payload_size; i++) {
        if ((i & 7u) == 0) {
            state = mix64(state + i);
        }
        payload[i] = (uint8_t)('a' + ((state >> ((i & 7u) * 8u)) % 26u));
    }
}

static int write_record(FILE *fp, const corpus_record_header_t *record,
                        const uint8_t *payload, uint32_t payload_size)
{
    return fwrite(record, sizeof(*record), 1, fp) == 1 &&
                   fwrite(payload, payload_size, 1, fp) == 1
               ? 0
               : -1;
}

static int write_corpus(const char *path, uint32_t kind, uint32_t rule_count,
                        uint32_t combo_count, uint32_t query_count,
                        uint32_t payload_size, uint32_t hit_bps,
                        uint32_t seed)
{
    uint32_t hit_count = (uint32_t)(((uint64_t)query_count * hit_bps) / 10000u);
    corpus_header_t header = {CORPUS_MAGIC, CORPUS_VERSION,
                              sizeof(corpus_header_t),
                              sizeof(corpus_record_header_t), query_count,
                              payload_size, hit_count, seed, kind, 0};
    uint32_t *order = NULL;
    uint8_t *payload = NULL;
    FILE *fp = NULL;
    uint32_t slot;
    int result = -1;

    if ((kind == KIND_HASH && payload_size != sizeof(tuple_t)) ||
        (kind != KIND_HASH && payload_size < 64u)) {
        fprintf(stderr, "invalid payload_size=%u for kind=%u\n",
                payload_size, kind);
        return -1;
    }
    if (kind == KIND_ACCOMBO &&
        (combo_count < 2u || combo_count * 2u > rule_count - combo_count)) {
        return -1;
    }
    order = make_query_order(query_count, seed);
    payload = (uint8_t *)malloc(payload_size);
    fp = fopen(path, "wb");
    if (order == NULL || payload == NULL || fp == NULL ||
        fwrite(&header, sizeof(header), 1, fp) != 1) {
        fprintf(stderr, "prepare corpus failed: %s errno=%d\n", path, errno);
        goto end;
    }

    for (slot = 0; slot < query_count; slot++) {
        uint32_t rank = order[slot];
        int hit = rank < hit_count;
        corpus_record_header_t record = {hit ? 1u : 0u, 0u, slot, 0u};

        if (kind == KIND_HASH) {
            tuple_t tuple;
            uint32_t index = (uint32_t)(mix64(((uint64_t)seed << 32) | rank) %
                                                rule_count);
            fill_tuple(&tuple, index, seed, !hit);
            (void)memcpy(payload, &tuple, sizeof(tuple));
            record.expected_candidates = record.expected_ids;
        } else {
            fill_filler(payload, payload_size, slot, seed);
            if (kind == KIND_AC) {
                if (hit) {
                    char literal[LITERAL_SIZE + 1u];
                    uint32_t index = (uint32_t)(
                        mix64(((uint64_t)seed << 32) | rank) % rule_count);
                    uint32_t room = payload_size - LITERAL_SIZE;
                    uint32_t pos = (uint32_t)(mix64(rank + seed) % (room + 1u));
                    make_literal(index, seed, literal);
                    (void)memcpy(payload + pos, literal, LITERAL_SIZE);
                    record.expected_candidates = 1u;
                }
            } else {
                char left_literal[LITERAL_SIZE + 1u];
                char right_literal[LITERAL_SIZE + 1u];
                uint32_t pair = (uint32_t)(
                    mix64(((uint64_t)seed << 32) | rank) % combo_count);
                uint32_t other = (pair + 1u +
                                  (uint32_t)(mix64(rank ^ seed) %
                                             (combo_count - 1u))) %
                                 combo_count;
                uint32_t left_index = pair * 2u;
                uint32_t right_index = hit ? left_index + 1u : other * 2u;
                uint32_t first_pos = 16u;
                uint32_t second_pos = payload_size - LITERAL_SIZE - 16u;

                make_literal(left_index, seed, left_literal);
                make_literal(right_index, seed, right_literal);
                (void)memcpy(payload + first_pos, left_literal, LITERAL_SIZE);
                (void)memcpy(payload + second_pos, right_literal, LITERAL_SIZE);
                record.expected_candidates = 2u;
            }
        }
        if (write_record(fp, &record, payload, payload_size) != 0) {
            fprintf(stderr, "write corpus record failed: %s slot=%u\n",
                    path, slot);
            goto end;
        }
    }
    result = 0;

end:
    if (fp != NULL && fclose(fp) != 0) {
        result = -1;
    }
    free(payload);
    free(order);
    return result;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s -a hash|ac|accombo -n rules [-c combos] [-q queries] "
            "[-H hit_bps] [-S seed] [-L payload] [-t rules_file] "
            "[-s corpus_file]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *algorithm = NULL;
    const char *rules_path = NULL;
    const char *corpus_path = NULL;
    uint32_t rule_count = 0;
    uint32_t combo_count = 0;
    uint32_t query_count = 0;
    uint32_t hit_bps = 0;
    uint32_t seed = 9202026u;
    uint32_t payload_size = 1024u;
    uint32_t kind = 0;
    int i;

    for (i = 1; i < argc; i += 2) {
        const char *option = argv[i];
        const char *value;

        if (i + 1 >= argc || option[0] != '-' || option[2] != '\0') {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        value = argv[i + 1];
        if (strcmp(option, "-a") == 0) {
            algorithm = value;
        } else if (strcmp(option, "-n") == 0) {
            rule_count = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-c") == 0) {
            combo_count = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-q") == 0) {
            query_count = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-H") == 0) {
            hit_bps = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-S") == 0) {
            seed = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-L") == 0) {
            payload_size = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(option, "-t") == 0) {
            rules_path = value;
        } else if (strcmp(option, "-s") == 0) {
            corpus_path = value;
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (algorithm == NULL || rule_count == 0 || hit_bps > 10000u ||
        (rules_path == NULL && corpus_path == NULL) ||
        (corpus_path != NULL && query_count == 0)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(algorithm, "hash") == 0) {
        kind = KIND_HASH;
        payload_size = sizeof(tuple_t);
    } else if (strcmp(algorithm, "ac") == 0) {
        kind = KIND_AC;
    } else if (strcmp(algorithm, "accombo") == 0) {
        kind = KIND_ACCOMBO;
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (rules_path != NULL) {
        int ret = kind == KIND_HASH
                      ? write_hash_rules(rules_path, rule_count, seed)
                      : kind == KIND_AC
                            ? write_ac_rules(rules_path, rule_count, seed)
                            : write_combo_rules(rules_path, rule_count,
                                                combo_count, seed);
        if (ret != 0) {
            return EXIT_FAILURE;
        }
    }
    if (corpus_path != NULL &&
        write_corpus(corpus_path, kind, rule_count, combo_count, query_count,
                     payload_size, hit_bps, seed) != 0) {
        return EXIT_FAILURE;
    }
    printf("generated algorithm=%s rules=%u combos=%u queries=%u hit_bps=%u "
           "payload=%u seed=%u\n",
           algorithm, rule_count, combo_count, query_count, hit_bps,
           payload_size, seed);
    return EXIT_SUCCESS;
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Megascan parameterized API and 920hg command entry.
 */
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "demo_common.h"
#include "api_session_demo.h"
#include "benchmark_case_demo.h"
#include "benchmark_runner.h"
#include "data_generator_runner.h"
#include "workflow_runner.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_help(const char *value)
{
    return strcmp(value, "-h") == 0 || strcmp(value, "--help") == 0;
}

static int parse_u32(const char *text, uint32_t *value, int allow_zero)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > UINT32_MAX || (!allow_zero && parsed == 0)) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int load_file(const char *path, uint8_t **data, uint32_t *length)
{
    FILE *file = fopen(path, "rb");
    long size;
    uint8_t *buffer;

    if (file == NULL) return -1;
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        (unsigned long)size > UINT32_MAX ||
        fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        return -1;
    }
    buffer = (uint8_t *)malloc(size == 0 ? 1u : (size_t)size);
    if (buffer == NULL ||
        (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size)) {
        free(buffer);
        (void)fclose(file);
        return -1;
    }
    (void)fclose(file);
    *data = buffer;
    *length = (uint32_t)size;
    return 0;
}

static void print_usage(const char *prog)
{
    printf("Megascan test CLI\n\n");
    printf("API:\n");
    printf("  %s apis\n", prog);
    printf("  %s --api <name> -h\n", prog);
    printf("  %s --api <name> <parameters...>\n", prog);
    printf("  %s session [command-file]\n", prog);
    printf("  %s modules | --module <name>\n\n", prog);
    printf("920hg (unchanged):\n");
    printf("  %s 920hg <subcommand...>\n", prog);
    printf("  %s benchmark <bench_920hg parameters...>\n", prog);
    printf("  %s generate-data <generator parameters...>\n", prog);
    printf("  %s corpus <options...>\n\n", prog);
    printf("Manual rule-file scan:\n");
    printf("  %s scan --matcher <ac|accombo|ac_legacy|ac_on_combo> "
           "--mode <block|stream-full> --rules-file <path> "
           "--rule-count <n> (--input <text>|--input-file <path>)\n", prog);
    printf("  %s lookup --rules-file <path> --rule-count <n> "
           "(--input-file <16-byte-key>|--corpus-file <path>)\n\n", prog);
    printf("Hard-coded scenarios, selftest and the fixed interactive menu were "
           "removed. Stateful session commands require explicit parameters.\n");
}

static void print_manual_help(const char *prog, const char *command)
{
    if (strcmp(command, "scan") == 0) {
        printf("%s scan --matcher <algorithm> --mode <block|stream-full> "
               "--rules-file <path> --rule-count <n> "
               "(--input <text>|--input-file <path>) [--chunk-size <n>]\n",
               prog);
    } else if (strcmp(command, "lookup") == 0) {
        printf("%s lookup --rules-file <path> --rule-count <n> "
               "(--input-file <path>|--corpus-file <path>)\n", prog);
    } else if (strcmp(command, "corpus") == 0 ||
               strcmp(command, "--benchmark-case") == 0) {
        printf("%s corpus --algorithm <algorithm> --mode <mode> "
               "--rules-file <path> --corpus-file <path> --rule-count <n> "
               "[--query-index <n>|--all-queries] [--chunk-size <n>]\n", prog);
    } else {
        print_usage(prog);
    }
}

typedef struct {
    demo_benchmark_case_t config;
    const char *input_text;
    const char *input_file;
} manual_args_t;

static int parse_manual_args(int argc, char **argv, int start,
                             manual_args_t *args)
{
    int i;

    for (i = start; i < argc; i++) {
        const char *option = argv[i];

        if (strcmp(option, "--all-queries") == 0) {
            args->config.all_queries = 1;
            continue;
        }
        if (++i >= argc) return -1;
        if (strcmp(option, "--algorithm") == 0 ||
            strcmp(option, "--matcher") == 0) {
            args->config.algorithm = argv[i];
        } else if (strcmp(option, "--mode") == 0) {
            args->config.mode = argv[i];
        } else if (strcmp(option, "--rules-file") == 0) {
            args->config.rules_file = argv[i];
        } else if (strcmp(option, "--corpus-file") == 0) {
            args->config.corpus_file = argv[i];
        } else if (strcmp(option, "--rule-count") == 0) {
            if (parse_u32(argv[i], &args->config.rule_count, 0) != 0) return -1;
        } else if (strcmp(option, "--query-index") == 0) {
            if (parse_u32(argv[i], &args->config.query_index, 1) != 0) return -1;
        } else if (strcmp(option, "--chunk-size") == 0) {
            if (parse_u32(argv[i], &args->config.chunk_size, 0) != 0) return -1;
        } else if (strcmp(option, "--input") == 0) {
            args->input_text = argv[i];
        } else if (strcmp(option, "--input-file") == 0) {
            args->input_file = argv[i];
        } else {
            return -1;
        }
    }
    return 0;
}

static int run_manual_data_command(const char *command, int argc, char **argv)
{
    manual_args_t args = {0};
    uint8_t *owned_input = NULL;
    const uint8_t *input = NULL;
    uint32_t input_len = 0;
    int corpus_mode = strcmp(command, "corpus") == 0 ||
                      strcmp(command, "--benchmark-case") == 0;
    int lookup_mode = strcmp(command, "lookup") == 0;
    int ret;

    if (parse_manual_args(argc, argv, 2, &args) != 0) {
        printf("invalid parameters\n");
        return 1;
    }
    if (lookup_mode) {
        args.config.algorithm = "hash";
        args.config.mode = "lookup";
    } else {
        if (args.config.algorithm == NULL) args.config.algorithm = "ac";
        if (args.config.mode == NULL) args.config.mode = "block";
    }
    if (args.config.rules_file == NULL || args.config.rule_count == 0) {
        printf("missing --rules-file or --rule-count\n");
        return 1;
    }
    if (corpus_mode || args.config.corpus_file != NULL) {
        if (args.config.corpus_file == NULL) {
            printf("missing --corpus-file\n");
            return 1;
        }
        return demo_run_benchmark_case(&args.config);
    }
    if (args.input_text != NULL && args.input_file != NULL) {
        printf("--input and --input-file are mutually exclusive\n");
        return 1;
    }
    if (args.input_text != NULL) {
        input = (const uint8_t *)args.input_text;
        input_len = (uint32_t)strlen(args.input_text);
    } else if (args.input_file != NULL &&
               load_file(args.input_file, &owned_input, &input_len) == 0) {
        input = owned_input;
    } else {
        printf("missing or unreadable input\n");
        return 1;
    }
    ret = demo_run_rule_file_scan(&args.config, input, input_len);
    free(owned_input);
    return ret;
}

int main(int argc, char **argv)
{
    const char *prog = argc > 0 ? argv[0] : "megascan_demo";
    int i;

    if (argc < 2) {
        print_usage(prog);
        return 1;
    }
    if (strcmp(argv[1], "benchmark") == 0) {
        for (i = 2; i < argc; i++) {
            if (is_help(argv[i])) {
                printf("%s benchmark <bench_920hg parameters...>\n", prog);
                return 0;
            }
        }
        return demo_run_920hg_benchmark(argc - 1, argv + 1);
    }
    if (strcmp(argv[1], "generate-data") == 0) {
        for (i = 2; i < argc; i++) {
            if (is_help(argv[i])) {
                printf("%s generate-data <gen_920hg_data parameters...>\n", prog);
                return 0;
            }
        }
        return demo_run_920hg_data_generator(argc - 1, argv + 1);
    }
    if (strcmp(argv[1], "920hg") == 0 || strcmp(argv[1], "--920hg") == 0) {
        return demo_run_920hg_workflow(argc - 1, argv + 1);
    }
    if (strcmp(argv[1], "--api") == 0) {
        if (argc < 3 || (argc == 3 && is_help(argv[2]))) {
            demo_print_api_cli_help(prog);
            return argc < 3 ? 1 : 0;
        }
        if (argc == 4 && is_help(argv[3])) {
            demo_print_api_test_help(prog, argv[2]);
            return 0;
        }
        return demo_run_api_cli(argv[2], argc - 3, argv + 3);
    }
    if (strcmp(argv[1], "session") == 0) {
        if (argc == 3 && is_help(argv[2])) {
            demo_print_api_session_help(prog);
            return 0;
        }
        if (argc > 3) {
            demo_print_api_session_help(prog);
            return 1;
        }
        return demo_run_api_session(argc == 3 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "apis") == 0 || strcmp(argv[1], "--apis") == 0) {
        demo_list_api_tests();
        return 0;
    }
    if (strcmp(argv[1], "modules") == 0) {
        demo_list_modules();
        return 0;
    }
    if (strcmp(argv[1], "--module") == 0) {
        if (argc != 3) return 1;
        return demo_show_module(argv[2]);
    }
    if (strcmp(argv[1], "scan") == 0 || strcmp(argv[1], "lookup") == 0 ||
        strcmp(argv[1], "corpus") == 0 ||
        strcmp(argv[1], "--benchmark-case") == 0) {
        if (argc == 3 && is_help(argv[2])) {
            print_manual_help(prog, argv[1]);
            return 0;
        }
        return run_manual_data_command(argv[1], argc, argv);
    }
    if (is_help(argv[1])) {
        print_usage(prog);
        return 0;
    }
    printf("unknown command: %s\n", argv[1]);
    print_usage(prog);
    return 1;
}

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Megascan parameterized API and 920hg command entry.
 */
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "demo_common.h"
#include "api_session_demo.h"
#include "benchmark_runner.h"
#include "data_generator_runner.h"
#include "flow_demo.h"
#include "workflow_runner.h"

#include <stdio.h>
#include <string.h>

static int is_help(const char *value)
{
    return strcmp(value, "-h") == 0 || strcmp(value, "--help") == 0;
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
    printf("  %s flows | flow <name>\n\n", prog);
    printf("920hg (unchanged):\n");
    printf("  %s 920hg <subcommand...>\n", prog);
    printf("  %s benchmark <bench_920hg parameters...>\n", prog);
    printf("  %s generate-data <generator parameters...>\n", prog);
    printf("\nHard-coded scenarios and automatic rule-file fixtures were removed.\n");
    printf("Use --api for one-shot calls or session for explicit object state.\n");
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
    if (strcmp(argv[1], "flows") == 0) {
        demo_list_flows();
        return 0;
    }
    if (strcmp(argv[1], "flow") == 0) {
        if (argc != 3) {
            demo_list_flows();
            return argc == 2 ? 0 : 1;
        }
        return demo_print_flow(argv[2]);
    }
    if (is_help(argv[1])) {
        print_usage(prog);
        return 0;
    }
    printf("unknown command: %s\n", argv[1]);
    print_usage(prog);
    return 1;
}

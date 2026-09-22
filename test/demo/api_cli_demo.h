/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MEGASCAN_TEST_API_CLI_DEMO_H
#define MEGASCAN_TEST_API_CLI_DEMO_H

/* Return 0 for success, 1 for failure, or -1 for an unknown API. */
int demo_run_api_command(const char *api, int argc, char **argv);

/* Return non-zero when detailed parameterized help was printed. */
int demo_print_api_command_help(const char *prog, const char *api);

#endif

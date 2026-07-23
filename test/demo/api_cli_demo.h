/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MEGASCAN_TEST_API_CLI_DEMO_H
#define MEGASCAN_TEST_API_CLI_DEMO_H

/*
 * Return -1 when @api is not handled by the extended CLI dispatcher.
 * Otherwise return a process-style status: 0 for success, 1 for failure.
 */
int demo_run_api_cli_extended(const char *api, int argc, char **argv);

/* Return non-zero when detailed parameterized help was printed. */
int demo_print_api_cli_extended_help(const char *prog, const char *api);

/* Print the extended API names and their compact syntax. */
void demo_print_api_cli_extended_index(void);

#endif

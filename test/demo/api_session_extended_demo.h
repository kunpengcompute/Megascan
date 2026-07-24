/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MEGASCAN_API_SESSION_EXTENDED_DEMO_H
#define MEGASCAN_API_SESSION_EXTENDED_DEMO_H

void demo_extended_session_reset(void);
int demo_extended_session_name_in_use(const char *name);
int demo_extended_session_dispatch(int argc, char **argv);
void demo_extended_session_list_handles(void);
void demo_extended_session_print_help(void);
int demo_extended_session_cleanup(void);

#endif

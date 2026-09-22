/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_RULE_INTERNAL_H
#define MS_RULE_INTERNAL_H

#include "ms_common.h"
#include "ms_database_internal.h"

int ms_rule_add_direct(ms_handler_t *handler, ms_rule_t *rule,
                       uint32_t rule_num);
int ms_rule_find_direct(ms_handler_t *handler, ms_rule_t *rule);
int ms_rule_remove_direct(ms_handler_t *handler, ms_rule_t *rule,
                          uint32_t rule_num);
int ms_rule_add_database_direct(ms_database_t *db, ms_rule_t *rule,
                                uint32_t rule_num);
int ms_rule_find_database_direct(const ms_database_t *db, ms_rule_t *rule);
int ms_stream_scan_direct(ms_stream_t *stream, const uint8_t *data,
                          uint32_t len);

#endif

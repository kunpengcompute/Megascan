/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_RULE_INTERNAL_H
#define MMPM_RULE_INTERNAL_H

#include "mmpm_common.h"
#include "mmpm_database_internal.h"

int mmpm_rule_add_direct(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);
int mmpm_rule_find_direct(mmpm_handler_t *handler, mmpm_rule_t *rule);
int mmpm_rule_remove_direct(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);
int mmpm_rule_add_database_direct(mmpm_database_t *db, mmpm_rule_t *rule, uint32_t rule_num);
int mmpm_rule_find_database_direct(const mmpm_database_t *db, mmpm_rule_t *rule);
int mmpm_stream_scan_direct(ms_stream_t *stream, const uint8_t *data, uint32_t len);

#endif

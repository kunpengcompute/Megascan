/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_ACDB_H
#define MS_ACDB_H

#include <stdint.h>

#include "ms_ac.h"
#include "ms_rule.h"

typedef struct ms_acdb ms_acdb_t;

int ms_acdb_create(ms_acdb_t **acdb);
void ms_acdb_destroy(ms_acdb_t *acdb);
int ms_acdb_build(ms_acdb_t *acdb, const ms_rule_t *rules, uint32_t rule_count);
int ms_acdb_match(const ms_acdb_t *acdb, const uint8_t *data, uint32_t len);
int ms_acdb_match_state_cb(const ms_acdb_t *acdb,
                           const uint8_t *data,
                           uint32_t len,
                           uint32_t *state,
                           ms_ac_match_cb cb,
                           void *ctx);
uint64_t ms_acdb_generation(const ms_acdb_t *acdb);

#endif

/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_RULE_H
#define MMPM_RULE_H

#include "mmpm_common.h"

/**
 * Register hash calculation function.
 *
 * Compatibility note: this configures the legacy mutable handler path.
 * Database-style APIs should keep HASH configuration attached to the database
 * lifecycle instead of mutating runtime scan objects.
 *
 * @handler: Matching engine handle.
 * @hash_func: Hash function pointer.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_set_hash_func(mmpm_handler_t *handler, hash_calc_func hash_func);

/**
 * Import fixed-string rules.
 *
 * Compatibility note: this mutates the legacy handler in place.
 * New external-memory code should prefer compiling a database object, then
 * scanning through database/scratch/stream APIs.
 *
 * @handler: Matching engine handle.
 * @rule: Rules to be imported.
 * @rule_num: Number of rules, needs to be consistent with the parameter @rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_add_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);

/**
 * Delete target rules.
 *
 * Compatibility note: this is part of the legacy mutable-handler workflow.
 * Database objects are intended to represent compiled state, so removal here
 * should be treated as compatibility behavior during the migration.
 *
 * Fixed-string rules are removed by key/key_len and alias. Combination rules
 * have no fixed-string key; pass their alias with key set to NULL and key_len
 * set to 0.
 *
 * @handler: Matching engine handle.
 * @rule: Rules to be deleted.
 * @rule_num: Number of rules, needs to be consistent with the parameter @rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_remove_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);

/**
 * Match rules against one complete block.
 *
 * Compatibility note: this is the legacy handler query entry. New database
 * call sites should prefer mmpm_find_database_rule() for direct rule lookup
 * or mmpm_scan_database() for block scanning semantics.
 *
 * For AC handlers, this uses the expression runtime so plain and combination
 * reporting follows the same semantics as mmpm_scan_expr().
 *
 * @handler: Matching engine handle.
 * @rule: Pending matching rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_find_rule(mmpm_handler_t *handler, mmpm_rule_t *rule);

/**
 * Scan data on an opened AC stream.
 *
 * This is a compatibility wrapper around the expression stream runtime. It
 * returns whether the current chunk produced any reported rule ID.
 *
 * @stream: Opened stream handle.
 * @data: Pending matching data.
 * @len: Length of pending matching data.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_scan_rule_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len);

#endif

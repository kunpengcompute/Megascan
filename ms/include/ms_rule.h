/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_RULE_H
#define MS_RULE_H

#include "ms_handler.h"

/**
 * Register hash calculation function.
 *
 * Compatibility note: this API belongs to the legacy handler path.
 *
 * @handler: Matching engine handle.
 * @hash_func: Hash function pointer.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_set_hash_func(ms_handler_t *handler, hash_calc_func hash_func);

/**
 * Import fixed-string rules.
 *
 * Compatibility note: this API belongs to the legacy handler path.
 *
 * @handler: Matching engine handle.
 * @rule: Rules to be imported.
 * @rule_num: Number of rules, needs to be consistent with the parameter @rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_add_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num);

/**
 * Delete target rules.
 *
 * Fixed-string rules are removed by key/key_len and alias. Combination rules
 * have no fixed-string key; pass their alias with key set to NULL and key_len
 * set to 0.
 *
 * Compatibility note: this API belongs to the legacy handler path.
 *
 * @handler: Matching engine handle.
 * @rule: Rules to be deleted.
 * @rule_num: Number of rules, needs to be consistent with the parameter @rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_remove_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num);

/**
 * Match rules against one complete block.
 *
 * For AC handlers, this uses the expression runtime so plain and combination
 * reporting follows the same semantics as ms_scan_expr().
 *
 * Compatibility note: database-style callers should prefer
 * ms_find_database_rule() or ms_scan_database().
 *
 * @handler: Matching engine handle.
 * @rule: Pending matching rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_find_rule(ms_handler_t *handler, ms_rule_t *rule);

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
int ms_scan_rule_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len);

#endif

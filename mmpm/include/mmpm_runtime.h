/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_RUNTIME_H
#define MMPM_RUNTIME_H

#include "mmpm_common.h"

/**
 * Scan data with expression-style rules.
 *
 * Compatibility note: this is the legacy handler-based block entry. New code
 * should prefer mmpm_scan_database() with an explicit scratch workspace.
 *
 * Non-quiet plain rules and matched combination rules are written to @ids.
 * Quiet plain rules participate in combination evaluation but are not written.
 * Quiet combination rules are accepted at compile time and skipped.
 * If @id_capacity is too small, @id_count is set to the required count and
 * MMPM_INVALID_PARA is returned.
 *
 * @handler: Matching engine handle. Only AC handlers are supported currently.
 * @data: Pending matching data.
 * @len: Length of pending matching data.
 * @ids: Output IDs. May be NULL only when @id_capacity is zero.
 * @id_count: Output ID count or required count on capacity failure.
 * @id_capacity: Capacity of @ids.
 * @return: 0 for success, MMPM_FIND_FAIL for no reported IDs, or an error code.
 */
int mmpm_scan_expr(mmpm_handler_t *handler, const uint8_t *data, uint32_t len,
                   uint32_t *ids, uint32_t *id_count, uint32_t id_capacity);

/**
 * Open a stream for AC matching.
 *
 * Compatibility note: this is the legacy handler-based stream entry. New code
 * should prefer mmpm_open_stream() together with explicit-scratch
 * stream scan/flush/close APIs.
 *
 * @handler: Matching engine handle.
 * @stream: Stream handle to be opened.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_open_handler_stream(mmpm_handler_t *handler, ms_stream_t **stream);

/**
 * Scan data on an opened AC stream with expression-style reporting.
 *
 * Compatibility note: this legacy API keeps expression temporary state inside
 * the stream path. New code should prefer mmpm_scan_stream().
 *
 * Plain rule state and combination state are accumulated across chunks for the
 * lifetime of the stream until reset. This API reports only the IDs newly
 * produced by the current chunk.
 *
 * @stream: Opened stream handle.
 * @data: Pending matching data.
 * @len: Length of pending matching data.
 * @ids: Output IDs. May be NULL only when @id_capacity is zero.
 * @id_count: Output ID count or required count on capacity failure.
 * @id_capacity: Capacity of @ids.
 * @return: 0 for success, MMPM_FIND_FAIL for no reported IDs, or an error code.
 */
int mmpm_scan_expr_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len,
                          uint32_t *ids, uint32_t *id_count, uint32_t id_capacity);

/**
 * Flush an expression stream at end of data.
 *
 * Compatibility note: new code should prefer mmpm_flush_stream().
 *
 * This reports combinations that can match without a positive sub-expression
 * hit in the current chunk, such as !101, using the stream's accumulated
 * logical state.
 */
int mmpm_scan_expr_stream_flush(ms_stream_t *stream,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity);

/**
 * Reset an opened AC stream to initial state.
 *
 * @stream: Opened stream handle.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_reset_stream(ms_stream_t *stream);

/**
 * Close an opened AC stream.
 *
 * Compatibility note: new code should prefer mmpm_close_stream().
 *
 * @stream: Opened stream handle.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_close_handler_stream(ms_stream_t *stream);

#endif

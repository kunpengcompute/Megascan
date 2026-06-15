/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>
#include <string.h>

#include "mmpm.h"
#include "mmpm_ac.h"
#include "mmpm_expr_internal.h"

int mmpm_expr_scan_stream(mmpm_handler_t *handler,
                          ACAutomaton *automaton,
                          uint32_t *node_index,
                          uint8_t *logical_vector,
                          uint32_t logical_count,
                          uint8_t *combination_vector,
                          uint32_t combination_count,
                          uint8_t *exhaustion_vector,
                          uint32_t exhaustion_count,
                          uint64_t *offset,
                          const uint8_t *data,
                          uint32_t len,
                          uint32_t *ids,
                          uint32_t *id_count,
                          uint32_t id_capacity)
{
    mmpm_compile_alias_capture_t capture = {0};
    uint32_t next_node_index;
    uint8_t *logical_vector_copy = NULL;
    uint8_t *combination_vector_copy = NULL;
    uint8_t *exhaustion_vector_copy = NULL;
    uint32_t logical_bytes;
    uint32_t combination_bytes;
    uint32_t exhaustion_bytes;
    int scan_ret;
    int ret;

    if (handler == NULL || automaton == NULL || node_index == NULL || offset == NULL ||
        data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    next_node_index = *node_index;
    logical_bytes = mmpm_compile_logical_byte_count(logical_count);
    combination_bytes = mmpm_compile_logical_byte_count(combination_count);
    exhaustion_bytes = mmpm_compile_logical_byte_count(exhaustion_count);
    if (logical_bytes != 0) {
        logical_vector_copy = (uint8_t *)mmpm_calloc(logical_bytes, sizeof(uint8_t));
        if (logical_vector_copy == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        (void)memcpy(logical_vector_copy, logical_vector, logical_bytes);
    }
    if (combination_bytes != 0) {
        combination_vector_copy = (uint8_t *)mmpm_calloc(combination_bytes, sizeof(uint8_t));
        if (combination_vector_copy == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        (void)memcpy(combination_vector_copy, combination_vector, combination_bytes);
    }
    if (exhaustion_bytes != 0) {
        exhaustion_vector_copy = (uint8_t *)mmpm_calloc(exhaustion_bytes, sizeof(uint8_t));
        if (exhaustion_vector_copy == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        (void)memcpy(exhaustion_vector_copy, exhaustion_vector, exhaustion_bytes);
    }

    *id_count = 0;
    scan_ret = ac_automaton_match_state_cb(automaton,
                                           data,
                                           len,
                                           &next_node_index,
                                           mmpm_compile_capture_alias,
                                           &capture);
    if (scan_ret != MMPM_SUCCESS && scan_ret != MMPM_FIND_FAIL) {
        ret = scan_ret;
        goto end;
    }

    ret = mmpm_compile_eval_stream_matches(handler,
                                           capture.aliases,
                                           capture.offsets,
                                           capture.count,
                                           logical_vector_copy,
                                           logical_count,
                                           combination_vector_copy,
                                           combination_count,
                                           exhaustion_vector_copy,
                                           exhaustion_count,
                                           *offset,
                                           *offset + len,
                                           false,
                                           false,
                                           ids,
                                           id_count,
                                           id_capacity);
    if (ret == MMPM_SUCCESS || ret == MMPM_FIND_FAIL) {
        *node_index = next_node_index;
        *offset += len;
        if (logical_bytes != 0) {
            (void)memcpy(logical_vector, logical_vector_copy, logical_bytes);
        }
        if (combination_bytes != 0) {
            (void)memcpy(combination_vector, combination_vector_copy, combination_bytes);
        }
        if (exhaustion_bytes != 0) {
            (void)memcpy(exhaustion_vector, exhaustion_vector_copy, exhaustion_bytes);
        }
    }

end:
    if (logical_vector_copy != NULL) {
        mmpm_free(logical_vector_copy);
    }
    if (combination_vector_copy != NULL) {
        mmpm_free(combination_vector_copy);
    }
    if (exhaustion_vector_copy != NULL) {
        mmpm_free(exhaustion_vector_copy);
    }
    if (capture.aliases != NULL) {
        mmpm_free(capture.aliases);
    }
    if (capture.offsets != NULL) {
        mmpm_free(capture.offsets);
    }
    return ret;
}

int mmpm_expr_flush_stream(mmpm_handler_t *handler,
                           uint8_t *logical_vector,
                           uint32_t logical_count,
                           uint8_t *combination_vector,
                           uint32_t combination_count,
                           uint8_t *exhaustion_vector,
                           uint32_t exhaustion_count,
                           uint64_t offset,
                           uint32_t *ids,
                           uint32_t *id_count,
                           uint32_t id_capacity)
{
    uint8_t *logical_vector_copy = NULL;
    uint8_t *combination_vector_copy = NULL;
    uint8_t *exhaustion_vector_copy = NULL;
    uint32_t logical_bytes;
    uint32_t combination_bytes;
    uint32_t exhaustion_bytes;
    int ret;

    if (handler == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    logical_bytes = mmpm_compile_logical_byte_count(logical_count);
    combination_bytes = mmpm_compile_logical_byte_count(combination_count);
    exhaustion_bytes = mmpm_compile_logical_byte_count(exhaustion_count);

    if (logical_bytes != 0) {
        logical_vector_copy = (uint8_t *)mmpm_calloc(logical_bytes, sizeof(uint8_t));
        if (logical_vector_copy == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        (void)memcpy(logical_vector_copy, logical_vector, logical_bytes);
    }
    if (combination_bytes != 0) {
        combination_vector_copy = (uint8_t *)mmpm_calloc(combination_bytes, sizeof(uint8_t));
        if (combination_vector_copy == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        (void)memcpy(combination_vector_copy, combination_vector, combination_bytes);
    }
    if (exhaustion_bytes != 0) {
        exhaustion_vector_copy = (uint8_t *)mmpm_calloc(exhaustion_bytes, sizeof(uint8_t));
        if (exhaustion_vector_copy == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        (void)memcpy(exhaustion_vector_copy, exhaustion_vector, exhaustion_bytes);
    }

    ret = mmpm_compile_eval_stream_matches(handler,
                                           NULL,
                                           NULL,
                                           0,
                                           logical_vector_copy,
                                           logical_count,
                                           combination_vector_copy,
                                           combination_count,
                                           exhaustion_vector_copy,
                                           exhaustion_count,
                                           offset,
                                           offset,
                                           true,
                                           false,
                                           ids,
                                           id_count,
                                           id_capacity);
    if (ret == MMPM_SUCCESS || ret == MMPM_FIND_FAIL) {
        if (logical_bytes != 0) {
            (void)memcpy(logical_vector, logical_vector_copy, logical_bytes);
        }
        if (combination_bytes != 0) {
            (void)memcpy(combination_vector, combination_vector_copy, combination_bytes);
        }
        if (exhaustion_bytes != 0) {
            (void)memcpy(exhaustion_vector, exhaustion_vector_copy, exhaustion_bytes);
        }
    }

end:
    if (logical_vector_copy != NULL) {
        mmpm_free(logical_vector_copy);
    }
    if (combination_vector_copy != NULL) {
        mmpm_free(combination_vector_copy);
    }
    if (exhaustion_vector_copy != NULL) {
        mmpm_free(exhaustion_vector_copy);
    }
    return ret;
}

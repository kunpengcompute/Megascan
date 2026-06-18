/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_DATABASE_H
#define MMPM_DATABASE_H

#include "mmpm_common.h"

typedef struct mmpm_database mmpm_database_t;
typedef struct mmpm_scratch mmpm_scratch_t;

/**
 * @brief 创建一个 database 对象
 *
 * database 表示编译后的长期匹配数据。当前迁移阶段与 mmpm_handler_t
 * 保持兼容布局，用于复用旧 handler API 的编译和销毁逻辑。
 *
 * @param db 输出 database 指针，调用前必须指向 NULL
 * @param type 匹配器类型，当前支持 MMPM_MATCHER_TYPE_HASH 和 MMPM_MATCHER_TYPE_AC
 * @param buckets HASH 主桶数量，AC 模式下可传 0
 * @param secondary_buckets HASH 二级桶数量，AC 模式下可传 0
 * @return MMPM_SUCCESS 表示成功，否则返回错误码
 */
int MMPM_CDECL mmpm_create_database(mmpm_database_t **db, mmpm_matcher_type type,
                                    uint32_t buckets, uint32_t secondary_buckets);

/**
 * @brief 编译多条表达式为 database
 *
 * 这是 mmpm_compile_database_multi_ext() 的简化形式，不传扩展约束。
 * HASH database 只支持普通固定匹配规则；AC database 支持组合规则。
 *
 * @param db 输出 database 指针，调用前必须指向 NULL
 * @param type 匹配器类型
 * @param buckets HASH 主桶数量
 * @param secondary_buckets HASH 二级桶数量
 * @param expressions 表达式数组
 * @param flags 每条表达式的标志位
 * @param ids 每条表达式对应的 ID
 * @param elements 表达式数量
 * @return MMPM_SUCCESS 表示成功，否则返回错误码
 */
int MMPM_CDECL mmpm_compile_database_multi(mmpm_database_t **db,
                                           mmpm_matcher_type type,
                                           uint32_t buckets,
                                           uint32_t secondary_buckets,
                                           const char *const *expressions,
                                           const uint32_t *flags,
                                           const uint32_t *ids,
                                           uint32_t elements);

/**
 * @brief 编译多条带扩展约束的表达式为 database
 *
 * 组合规则通过 MMPM_FLAG_COMBINATION 标识。当前 HASH 不支持组合规则，
 * 如果 HASH database 收到组合规则会返回 MMPM_NOT_SUPPORTED。
 *
 * @param db 输出 database 指针，调用前必须指向 NULL
 * @param type 匹配器类型
 * @param buckets HASH 主桶数量
 * @param secondary_buckets HASH 二级桶数量
 * @param expressions 表达式数组
 * @param flags 每条表达式的标志位
 * @param ids 每条表达式对应的 ID
 * @param ext 每条表达式的扩展约束，可为 NULL
 * @param elements 表达式数量
 * @return MMPM_SUCCESS 表示成功，否则返回错误码
 */
int MMPM_CDECL mmpm_compile_database_multi_ext(mmpm_database_t **db,
                                               mmpm_matcher_type type,
                                               uint32_t buckets,
                                               uint32_t secondary_buckets,
                                               const char *const *expressions,
                                               const uint32_t *flags,
                                               const uint32_t *ids,
                                               const mmpm_expr_ext_t *ext,
                                               uint32_t elements);

/**
 * @brief 释放 database 对象
 *
 * @param db 由 mmpm_create_database() 或 mmpm_compile_database_multi*() 创建的对象
 * @return MMPM_SUCCESS 表示成功，否则返回错误码
 */
int MMPM_CDECL mmpm_free_database(mmpm_database_t *db);

/**
 * @brief 为 database 分配 block scan 临时工作区
 *
 * scratch 用于保存一次 block scan 过程中的临时状态，例如逻辑向量、
 * 组合向量、耗尽向量以及 AC 命中捕获缓冲。内存走 Scratch 分配器。
 *
 * @param db 已编译或已创建的 database 对象
 * @param scratch 输出 scratch 指针，调用前必须指向 NULL
 * @return MMPM_SUCCESS 表示成功，否则返回错误码
 */
int MMPM_CDECL mmpm_alloc_scratch(const mmpm_database_t *db, mmpm_scratch_t **scratch);

/**
 * @brief 释放 scratch 工作区
 *
 * @param scratch 由 mmpm_alloc_scratch() 创建的对象
 * @return MMPM_SUCCESS 表示成功，否则返回错误码
 */
int MMPM_CDECL mmpm_free_scratch(mmpm_scratch_t *scratch);

/**
 * @brief 使用 database 和显式 scratch 执行 block scan
 *
 * 当前 scan database 只支持 AC database。HASH database 仍使用固定 key 查询
 * 接口，不承诺组合扫描语义。
 *
 * @param db 已编译的 AC database
 * @param data 输入数据
 * @param len 输入数据长度
 * @param scratch 由同一个 db 分配的 scratch
 * @param ids 输出命中的 ID
 * @param id_count 输出命中数量
 * @param id_capacity ids 数组容量
 * @return MMPM_SUCCESS 表示有命中，MMPM_FIND_FAIL 表示无命中，否则返回错误码
 */
int MMPM_CDECL mmpm_scan_database(const mmpm_database_t *db,
                                  const uint8_t *data,
                                  uint32_t len,
                                  mmpm_scratch_t *scratch,
                                  uint32_t *ids,
                                  uint32_t *id_count,
                                  uint32_t id_capacity);

/**
 * @brief 使用 database 执行单条规则查询
 *
 * HASH database 按固定 key 查询；AC database 走表达式运行时，因此普通规则
 * 和组合规则的返回语义与 mmpm_scan_database() 保持一致。
 *
 * @param db 已创建或已编译的 database
 * @param rule 输入待查规则；成功时 alias 会被更新为命中的 ID
 * @return MMPM_SUCCESS 表示有命中，MMPM_FIND_FAIL 表示无命中，否则返回错误码
 */
int MMPM_CDECL mmpm_find_database_rule(const mmpm_database_t *db, mmpm_rule_t *rule);

/**
 * @brief 基于 database 打开 stream
 *
 * 当前仅支持 AC database。stream 对象及其跨 chunk 状态内存走 Stream
 * 分配器。旧 handler 流接口应收敛为该完整入口的兼容封装。
 *
 * @param db 已编译的 AC database
 * @param stream 输出 stream 指针，调用前必须指向 NULL
 * @return MMPM_SUCCESS 表示成功，否则返回错误码
 */
int MMPM_CDECL mmpm_open_stream(const mmpm_database_t *db, ms_stream_t **stream);

/**
 * @brief 使用显式 scratch 扫描 database stream
 *
 * stream 只保存跨 chunk 状态；本次调用的捕获、向量副本和组合计算临时
 * 内存来自 scratch。scratch 必须由打开该 stream 的同一个 database 分配。
 */
int MMPM_CDECL mmpm_scan_stream(ms_stream_t *stream,
                                const uint8_t *data,
                                uint32_t len,
                                mmpm_scratch_t *scratch,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity);

/**
 * @brief 在流末尾执行组合规则计算但不释放 stream
 */
int MMPM_CDECL mmpm_flush_stream(ms_stream_t *stream,
                                 mmpm_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity);

/**
 * @brief 在流末尾执行组合规则计算并释放 stream
 *
 * 参数校验成功后，无论末尾计算是否成功，stream 都会被释放。
 * 没有末尾报告时 id_count 为 0，关闭仍返回 MMPM_SUCCESS。
 */
int MMPM_CDECL mmpm_close_stream(ms_stream_t *stream,
                                 mmpm_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity);

#endif

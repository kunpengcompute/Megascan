/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_ALLOCATOR_H
#define MS_ALLOCATOR_H

#include <stdlib.h>
#include <stdint.h>

/**
 * @brief 跨平台调用约定宏
 * 
 * 在 Windows 上使用 __cdecl 调用约定，确保回调函数的参数传递和栈清理方式正确。
 * 在 Linux/Unix 上默认就是 cdecl，无需额外声明。
 */
#if defined(_WIN32)
#define MS_CDECL __cdecl
#else
#define MS_CDECL
#endif

/**
 * @brief 内存分配函数指针类型
 * 
 * @param size 要分配的内存大小（字节）
 * @return 成功返回分配的内存指针，失败返回 NULL
 */
typedef void *(MS_CDECL *ms_alloc_t)(size_t size);

/**
 * @brief 内存分配并初始化函数指针类型
 * 
 * 与 malloc 类似，但会将分配的内存初始化为零。
 * 
 * @param nmemb 元素数量
 * @param size 每个元素的大小（字节）
 * @return 成功返回分配并清零的内存指针，失败返回 NULL
 */
typedef void *(MS_CDECL *ms_calloc_t)(size_t nmemb, size_t size);

/**
 * @brief 内存释放函数指针类型
 * 
 * @param ptr 要释放的内存指针（可以是 NULL）
 */
typedef void (MS_CDECL *ms_free_t)(void *ptr);

/**
 * @brief 检查内存地址是否对齐的宏
 * 
 * @param ptr 要检查的内存指针
 * @param align 对齐边界（必须是 2 的幂次方）
 * @return 如果地址对齐返回 true，否则返回 false
 * 
 * 原理：将指针转换为整数后取模，如果余数为 0 说明对齐。
 */
#define ISALIGNED_N(ptr, align) (((uintptr_t)(ptr)) % (align) == 0)

/**
 * @brief 数据库分配器 - 用于持久化数据的内存分配
 * 
 * 这些全局变量存储用户自定义的内存分配函数，默认为标准库的 malloc/calloc/free。
 * 用户可以通过 ms_set_allocator() 函数自定义这些函数。
 */
extern ms_alloc_t g_ms_db_alloc;
extern ms_calloc_t g_ms_db_calloc;
extern ms_free_t g_ms_db_free;

/**
 * @brief Scratch 分配器 - 用于扫描过程中的临时工作区内存
 *
 * 后续 ms_scratch_t 会使用这组分配器。当前保留为分类型内存接口的一部分。
 */
extern ms_alloc_t g_ms_scratch_alloc;
extern ms_calloc_t g_ms_scratch_calloc;
extern ms_free_t g_ms_scratch_free;

/**
 * @brief Stream 分配器 - 用于流模式状态内存
 *
 * 每条流的 ms_stream_t 及其跨 chunk 保存的运行时状态使用这组分配器。
 */
extern ms_alloc_t g_ms_stream_alloc;
extern ms_calloc_t g_ms_stream_calloc;
extern ms_free_t g_ms_stream_free;

/**
 * @brief Misc 分配器 - 用于杂项 API 输出或临时辅助内存
 *
 * 预留给错误信息、序列化结果、信息字符串等不属于 database/scratch/stream 的内存。
 */
extern ms_alloc_t g_ms_misc_alloc;
extern ms_calloc_t g_ms_misc_calloc;
extern ms_free_t g_ms_misc_free;

/**
 * @brief 检查内存分配是否成功
 * 
 * @param mem 要检查的内存指针
 * @return MS_SUCCESS 表示成功，MS_MALLOC_FAIL 表示内存分配失败，
 *         MS_INVALID_PARA 表示对齐检查失败
 */
int ms_check_alloc(const void *mem);

/**
 * @brief 获取当前的数据库分配函数
 * @return 当前的 malloc 函数指针
 */
ms_alloc_t ms_get_db_alloc(void);

/**
 * @brief 获取当前的数据库分配并初始化函数
 * @return 当前的 calloc 函数指针
 */
ms_calloc_t ms_get_db_calloc(void);

/**
 * @brief 获取当前的数据库释放函数
 * @return 当前的 free 函数指针
 */
ms_free_t ms_get_db_free(void);

/**
 * @brief 设置全局内存分配器
 * 
 * 用户可以通过此函数提供自定义的内存分配函数，实现对 MS 库内存分配的完全控制。
 * 如果传入 NULL，会自动回退到标准库函数。
 * 
 * @param alloc_func 自定义 malloc 函数
 * @param calloc_func 自定义 calloc 函数
 * @param free_func 自定义 free 函数
 * @return MS_SUCCESS 表示成功
 */
int MS_CDECL ms_set_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func, ms_free_t free_func);

/**
 * @brief 设置数据库内存分配器
 *
 * 这组分配器用于编译后长期存在的数据，如 ms_database_t、AC 自动机、
 * HASH 表和逻辑组合编译上下文。如果传入 NULL，会回退到标准库函数。
 *
 * @param alloc_func 自定义 malloc 函数
 * @param calloc_func 自定义 calloc 函数
 * @param free_func 自定义 free 函数
 * @return MS_SUCCESS 表示成功
 */
int MS_CDECL ms_set_database_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                                           ms_free_t free_func);

/**
 * @brief 设置 Scratch 内存分配器
 *
 * 这组分配器用于扫描临时工作区。当前接口已预留，后续 ms_scratch_t 会使用。
 * 如果传入 NULL，会回退到标准库函数。
 *
 * @param alloc_func 自定义 malloc 函数
 * @param calloc_func 自定义 calloc 函数
 * @param free_func 自定义 free 函数
 * @return MS_SUCCESS 表示成功
 */
int MS_CDECL ms_set_scratch_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                                          ms_free_t free_func);

/**
 * @brief 设置 Stream 内存分配器
 *
 * 这组分配器用于流模式状态对象及其跨 chunk 保存的运行时状态。
 * 如果传入 NULL，会回退到标准库函数。
 *
 * @param alloc_func 自定义 malloc 函数
 * @param calloc_func 自定义 calloc 函数
 * @param free_func 自定义 free 函数
 * @return MS_SUCCESS 表示成功
 */
int MS_CDECL ms_set_stream_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                                         ms_free_t free_func);

/**
 * @brief 设置 Misc 内存分配器
 *
 * 这组分配器用于杂项 API 输出或临时辅助内存。当前作为分类型外部内存模型预留。
 * 如果传入 NULL，会回退到标准库函数。
 *
 * @param alloc_func 自定义 malloc 函数
 * @param calloc_func 自定义 calloc 函数
 * @param free_func 自定义 free 函数
 * @return MS_SUCCESS 表示成功
 */
int MS_CDECL ms_set_misc_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                                       ms_free_t free_func);

/**
 * @brief MS 封装的 malloc 函数
 * 
 * 使用当前设置的数据库分配器进行内存分配。
 * 
 * @param size 要分配的内存大小（字节）
 * @return 成功返回分配的内存指针，失败返回 NULL
 */
void *ms_malloc(size_t size);

/**
 * @brief MS 封装的 calloc 函数
 * 
 * 使用当前设置的数据库分配器进行内存分配并初始化为零。
 * 
 * @param nmemb 元素数量
 * @param size 每个元素的大小（字节）
 * @return 成功返回分配并清零的内存指针，失败返回 NULL
 */
void *ms_calloc(size_t nmemb, size_t size);

/**
 * @brief MS 封装的 free 函数
 * 
 * 使用当前设置的数据库分配器释放内存。
 * 
 * @param ptr 要释放的内存指针（可以是 NULL）
 */
void ms_free(void *ptr);

/**
 * @brief 使用数据库分配器分配内存
 *
 * @param size 要分配的内存大小（字节）
 * @return 成功返回分配的内存指针，失败返回 NULL
 */
void *ms_database_malloc(size_t size);

/**
 * @brief 使用数据库分配器分配并清零内存
 *
 * @param nmemb 元素数量
 * @param size 每个元素的大小（字节）
 * @return 成功返回分配并清零的内存指针，失败返回 NULL
 */
void *ms_database_calloc(size_t nmemb, size_t size);

/**
 * @brief 使用数据库释放函数释放内存
 *
 * @param ptr 要释放的内存指针（可以是 NULL）
 */
void ms_database_free(void *ptr);

/**
 * @brief 使用 Scratch 分配器分配内存
 *
 * @param size 要分配的内存大小（字节）
 * @return 成功返回分配的内存指针，失败返回 NULL
 */
void *ms_scratch_malloc(size_t size);

/**
 * @brief 使用 Scratch 分配器分配并清零内存
 *
 * @param nmemb 元素数量
 * @param size 每个元素的大小（字节）
 * @return 成功返回分配并清零的内存指针，失败返回 NULL
 */
void *ms_scratch_calloc(size_t nmemb, size_t size);

/**
 * @brief 使用 Scratch 释放函数释放内存
 *
 * @param ptr 要释放的内存指针（可以是 NULL）
 */
void ms_scratch_free(void *ptr);

/**
 * @brief 使用 Stream 分配器分配内存
 *
 * @param size 要分配的内存大小（字节）
 * @return 成功返回分配的内存指针，失败返回 NULL
 */
void *ms_stream_malloc(size_t size);

/**
 * @brief 使用 Stream 分配器分配并清零内存
 *
 * @param nmemb 元素数量
 * @param size 每个元素的大小（字节）
 * @return 成功返回分配并清零的内存指针，失败返回 NULL
 */
void *ms_stream_calloc(size_t nmemb, size_t size);

/**
 * @brief 使用 Stream 释放函数释放内存
 *
 * @param ptr 要释放的内存指针（可以是 NULL）
 */
void ms_stream_free(void *ptr);

/**
 * @brief 使用 Misc 分配器分配内存
 *
 * @param size 要分配的内存大小（字节）
 * @return 成功返回分配的内存指针，失败返回 NULL
 */
void *ms_misc_malloc(size_t size);

/**
 * @brief 使用 Misc 分配器分配并清零内存
 *
 * @param nmemb 元素数量
 * @param size 每个元素的大小（字节）
 * @return 成功返回分配并清零的内存指针，失败返回 NULL
 */
void *ms_misc_calloc(size_t nmemb, size_t size);

/**
 * @brief 使用 Misc 释放函数释放内存
 *
 * @param ptr 要释放的内存指针（可以是 NULL）
 */
void ms_misc_free(void *ptr);

/**
 * @brief 使用数据库分配器分配对齐内存
 *
 * 用于 HASH 等需要固定对齐边界的数据库内部结构。释放时必须使用
 * ms_database_aligned_free()。
 *
 * @param size 要分配的内存大小（字节）
 * @param align 对齐边界，必须是 2 的幂
 * @return 成功返回按 align 对齐的内存指针，失败返回 NULL
 */
void *ms_database_aligned_alloc(size_t size, size_t align);

/**
 * @brief 释放数据库对齐分配得到的内存
 *
 * @param ptr ms_database_aligned_alloc() 返回的指针（可以是 NULL）
 */
void ms_database_aligned_free(void *ptr);

#endif

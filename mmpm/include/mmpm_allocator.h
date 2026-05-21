/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_ALLOCATOR_H
#define MMPM_ALLOCATOR_H

#include <stdlib.h>
#include <stdint.h>

/**
 * @brief 跨平台调用约定宏
 * 
 * 在 Windows 上使用 __cdecl 调用约定，确保回调函数的参数传递和栈清理方式正确。
 * 在 Linux/Unix 上默认就是 cdecl，无需额外声明。
 */
#if defined(_WIN32)
#define MMPM_CDECL __cdecl
#else
#define MMPM_CDECL
#endif

/**
 * @brief 内存分配函数指针类型
 * 
 * @param size 要分配的内存大小（字节）
 * @return 成功返回分配的内存指针，失败返回 NULL
 */
typedef void *(MMPM_CDECL *mmpm_alloc_t)(size_t size);

/**
 * @brief 内存分配并初始化函数指针类型
 * 
 * 与 malloc 类似，但会将分配的内存初始化为零。
 * 
 * @param nmemb 元素数量
 * @param size 每个元素的大小（字节）
 * @return 成功返回分配并清零的内存指针，失败返回 NULL
 */
typedef void *(MMPM_CDECL *mmpm_calloc_t)(size_t nmemb, size_t size);

/**
 * @brief 内存释放函数指针类型
 * 
 * @param ptr 要释放的内存指针（可以是 NULL）
 */
typedef void (MMPM_CDECL *mmpm_free_t)(void *ptr);

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
 * 用户可以通过 mmpm_set_allocator() 函数自定义这些函数。
 */
extern mmpm_alloc_t g_mmpm_db_alloc;
extern mmpm_calloc_t g_mmpm_db_calloc;
extern mmpm_free_t g_mmpm_db_free;

/**
 * @brief 检查内存分配是否成功
 * 
 * @param mem 要检查的内存指针
 * @return MMPM_SUCCESS 表示成功，MMPM_MALLOC_FAIL 表示内存分配失败，
 *         MMPM_INVALID_PARA 表示对齐检查失败
 */
int mmpm_check_alloc(const void *mem);

/**
 * @brief 获取当前的数据库分配函数
 * @return 当前的 malloc 函数指针
 */
mmpm_alloc_t mmpm_get_db_alloc(void);

/**
 * @brief 获取当前的数据库分配并初始化函数
 * @return 当前的 calloc 函数指针
 */
mmpm_calloc_t mmpm_get_db_calloc(void);

/**
 * @brief 获取当前的数据库释放函数
 * @return 当前的 free 函数指针
 */
mmpm_free_t mmpm_get_db_free(void);

/**
 * @brief 设置全局内存分配器
 * 
 * 用户可以通过此函数提供自定义的内存分配函数，实现对 MMPM 库内存分配的完全控制。
 * 如果传入 NULL，会自动回退到标准库函数。
 * 
 * @param alloc_func 自定义 malloc 函数
 * @param calloc_func 自定义 calloc 函数
 * @param free_func 自定义 free 函数
 * @return MMPM_SUCCESS 表示成功
 */
int MMPM_CDECL mmpm_set_allocator(mmpm_alloc_t alloc_func, mmpm_calloc_t calloc_func, mmpm_free_t free_func);

/**
 * @brief MMPM 封装的 malloc 函数
 * 
 * 使用当前设置的数据库分配器进行内存分配。
 * 
 * @param size 要分配的内存大小（字节）
 * @return 成功返回分配的内存指针，失败返回 NULL
 */
void *mmpm_malloc(size_t size);

/**
 * @brief MMPM 封装的 calloc 函数
 * 
 * 使用当前设置的数据库分配器进行内存分配并初始化为零。
 * 
 * @param nmemb 元素数量
 * @param size 每个元素的大小（字节）
 * @return 成功返回分配并清零的内存指针，失败返回 NULL
 */
void *mmpm_calloc(size_t nmemb, size_t size);

/**
 * @brief MMPM 封装的 free 函数
 * 
 * 使用当前设置的数据库分配器释放内存。
 * 
 * @param ptr 要释放的内存指针（可以是 NULL）
 */
void mmpm_free(void *ptr);

#endif
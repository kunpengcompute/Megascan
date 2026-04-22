/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Replace source function address with mock function.
 *              Not work well for system functions (can't Restore after mock),
 *              including "linux's open", so try your best to avoid mock "open".
 */
#ifndef KTEST_MOCK_H
#define KTEST_MOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>

#if defined(__aarch64__) || defined(_M_ARM64)
    #define CODESIZE 16U
    #define CODESIZE_MIN 16U
    #define CODESIZE_MAX CODESIZE

    #define SWAP_ADDR_FAR(ADDR, MOCK_ADDR) \
    do { \
        ((uint32_t*)ADDR)[0] = 0x58000040 | 9; \
        ((uint32_t*)ADDR)[1] = 0xd61f0120 | (9 << 5); \
        *(long long *)(ADDR + 8) = (uintptr_t)MOCK_ADDR; \
        __builtin___clear_cache(ADDR, ADDR + CODESIZE); \
    } while (0)
    #define SWAP_ADDR_NEAR(ADDR, MOCK_ADDR) SWAP_ADDR_FAR(ADDR, MOCK_ADDR)
#elif defined(__arm__) || defined(_M_ARM)
    #define CODESIZE 8U
    #define CODESIZE_MIN 8U
    #define CODESIZE_MAX CODESIZE

    #define SWAP_ADDR_FAR(ADDR, MOCK_ADDR) \
    do { \
        ((uint32_t*)ADDR)[0] = 0xe51ff004; \
        ((uint32_t*)ADDR)[1] = (uintptr_t)MOCK_ADDR; \
        __builtin___clear_cache(ADDR, ADDR + CODESIZE); \
    } while (0)
    #define SWAP_ADDR_NEAR(ADDR, MOCK_ADDR) SWAP_ADDR_FAR(ADDR, MOCK_ADDR)
#else
    #define CODESIZE 13U
    #define CODESIZE_MIN 5U
    #define CODESIZE_MAX CODESIZE

    #define SWAP_ADDR_FAR(ADDR, MOCK_ADDR) \
    do { \
        *(ADDR) = 0x49; \
        *((ADDR) + 1) = 0xbb; \
        *(long long *)((ADDR) + 2) = (uintptr_t)(MOCK_ADDR); \
        *((ADDR) + 10) = 0x41; \
        *((ADDR) + 11) = 0xff; \
        *((ADDR) + 12) = 0xe3; \
    } while (0)

    #define SWAP_ADDR_NEAR(ADDR, MOCK_ADDR) \
    do { \
        *(ADDR) = 0xE9; \
        *(int *)((ADDR) + 1) = (int)((MOCK_ADDR) - (ADDR) - CODESIZE_MIN); \
    } while (0)
#endif

typedef void (*MOCK_CREATE)(void *, void *);
typedef void (*MOCK_DELETE)(void *);
typedef uint8_t *FuncAddr;

typedef struct stFuncInfo {
    struct stFuncInfo *next;
    FuncAddr addr;
    uint8_t body[CODESIZE_MAX];
    bool farJmp;
} SrcFuncInfo;

static void KTestMockCreate(void *func, void *mockFunc);
static void KTestMockDelete(void *func);

#ifdef __cplusplus
template<typename T>
FuncAddr GetAddr(T func)
{
    union {
        T func;
        FuncAddr addr;
    } func2Addr;
    func2Addr.func = func;
    return func2Addr.addr;
}
    class KTestMock {
    public:
        KTestMock(){};
        ~KTestMock(){};
        template<typename T, typename S>
        void Create(T func, S mockFunc)
        {
            KTestMockCreate(GetAddr(func), GetAddr(mockFunc));
        }
        template<typename T>
        void Delete(T func)
        {
            KTestMockDelete(GetAddr(func));
        }
        SrcFuncInfo *funcInfo = NULL;
    };
#else
static FuncAddr GetAddr(void *func)
{
    return (FuncAddr)func;
}
typedef struct {
    MOCK_CREATE Create;
    MOCK_DELETE Delete;
    SrcFuncInfo *funcInfo;
} KTestMock;
#endif

static KTestMock *g_NowMock = NULL;
static long g_pagesize = 0;

static bool IsAddressFar(FuncAddr addr, FuncAddr mockAddr)
{
    uintptr_t distance = (mockAddr >= addr) ? (mockAddr - addr) : (addr - mockAddr);
    return (sizeof(FuncAddr) > 4) && (((distance >> 31) - 1) > 0); /* 右移4字节, 右移31字节获取地址头 */
}

static void StoreFunc(SrcFuncInfo *funcInfo)
{
    size_t size = funcInfo->farJmp ? CODESIZE_MAX : CODESIZE_MIN;
    for (int i = 0; i < size; i++) {
        funcInfo->body[i] = funcInfo->addr[i];
    }
}

static void RestoreFunc(SrcFuncInfo *funcInfo)
{
    size_t size = funcInfo->farJmp ? CODESIZE_MAX : CODESIZE_MIN;
    for (int i = 0; i < size; i++) {
        funcInfo->addr[i] = funcInfo->body[i];
    }
    __builtin___clear_cache(funcInfo->addr, funcInfo->addr + size);
}

static void SwapAddr(bool farJmp, FuncAddr addr, FuncAddr mockAddr)
{
    if (farJmp) {
        SWAP_ADDR_FAR(addr, mockAddr);
    } else {
        SWAP_ADDR_NEAR(addr, mockAddr);
    }
}

static FuncAddr GetPage(FuncAddr addr)
{
    return (FuncAddr)((uintptr_t)addr & ~(g_pagesize - 1));
}

static void KTestMockCreate(void *func, void *mockFunc)
{
    FuncAddr addr = GetAddr(func);
    FuncAddr mockAddr = GetAddr(mockFunc);
    SrcFuncInfo *funcInfo = (SrcFuncInfo *)malloc(sizeof(SrcFuncInfo));
    if (funcInfo == NULL) {
        return;
    }
    funcInfo->addr = addr;
    funcInfo->farJmp = IsAddressFar(addr, mockAddr);
    funcInfo->next = NULL;
    StoreFunc(funcInfo);
    if (mprotect(GetPage(funcInfo->addr), g_pagesize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        printf("mprotect failed, errno:%d\r\n", errno);
        return;
    }
    SwapAddr(funcInfo->farJmp, addr, mockAddr);
    if (mprotect(GetPage(funcInfo->addr), g_pagesize, PROT_READ | PROT_EXEC) != 0) {
        printf("mprotect failed, errno:%d\r\n", errno);
        return;
    }
    SrcFuncInfo *preFuncInfo = g_NowMock->funcInfo;
    if (g_NowMock->funcInfo == NULL) {
        g_NowMock->funcInfo = funcInfo;
        return;
    }
    while (preFuncInfo->next != NULL) {
        preFuncInfo = preFuncInfo->next;
    }
    preFuncInfo->next = funcInfo;
}

static void KTestMockDelete(void *func)
{
    FuncAddr addr = GetAddr(func);
    SrcFuncInfo *preFuncInfo = g_NowMock->funcInfo;
    SrcFuncInfo *funcInfo = NULL;
    if (preFuncInfo->addr == addr) {
        funcInfo = preFuncInfo;
        g_NowMock->funcInfo = preFuncInfo->next;
    } else {
        while (preFuncInfo->next) {
            if (preFuncInfo->next->addr == addr) {
                funcInfo = preFuncInfo->next;
                preFuncInfo->next = preFuncInfo->next->next;
                break;
            }
        }
    }
    if (mprotect(GetPage(funcInfo->addr), g_pagesize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        printf("mprotect failed, errno:%d\r\n", errno);
        return;
    }
    RestoreFunc(funcInfo);
    if (mprotect(GetPage(funcInfo->addr), g_pagesize, PROT_READ | PROT_EXEC) != 0) {
        printf("mprotect failed, errno:%d\r\n", errno);
        return;
    }
    free(funcInfo);
}

static KTestMock *CreateMock()
{
    g_pagesize = sysconf(_SC_PAGE_SIZE);
    if (g_pagesize <= 0) {
        g_pagesize = 4096; /* 默认page大小为4096 */
    }
    if (g_NowMock != NULL) {
        free(g_NowMock);
    }
#ifdef __cplusplus
    g_NowMock = new KTestMock();
#else
    g_NowMock = (KTestMock *)malloc(sizeof(KTestMock));
    g_NowMock->Create = KTestMockCreate;
    g_NowMock->Delete = KTestMockDelete;
    g_NowMock->funcInfo = NULL;
#endif
    return g_NowMock;
}

static void DeleteMock()
{
    SrcFuncInfo *nowFuncInfo = g_NowMock->funcInfo;
    while (nowFuncInfo) {
        SrcFuncInfo *nextFuncInfo = nowFuncInfo->next;
        KTestMockDelete(nowFuncInfo->addr);
        nowFuncInfo = nextFuncInfo;
    }
    if (g_NowMock != NULL) {
#ifdef __cplusplus
        delete(g_NowMock);
#else
        free(g_NowMock);
#endif
        g_NowMock = NULL;
    }
}

#endif
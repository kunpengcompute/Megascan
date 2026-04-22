/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Useful mock functions.
 */
#ifndef KTEST_MOCK_FUNC_H
#define KTEST_MOCK_FUNC_H

#ifdef __cplusplus

#include <iostream>
#include <vector>
#include <queue>

#define MOCK_RETURN(retValue, retType, funcName) static retType funcName() {return retValue;}

namespace KTest {
    void Void(){};
/**
 * specific return value of typename T, support basic types like int, bool, string...
 * and pointer, but may not support object currently.
 */
template<typename T>
class MockFunc {
public:
    /**
     * "SetReturn & GetReturn" mock can only return value once of type T in one case.
     * In one case, if you need to return different value of type T, you can use
     * "SetReturnQueue & PopReturn" instead.
    */
    static void SetReturn(T ret)
    {
        GetInstance().m_ret = ret;
    }
    static T GetReturn()
    {
        return GetInstance().m_ret;
    }

    /**
     * Set a sequence return values of type T, and "PopReturn" value one by one during runing case.
     * for example: SetReturnQueue<int>({1, 1, -1, 0});
     * Attention: The queue will be clear when call SetReturnQueue each time.
    */
    static void SetReturnQueue(const std::vector<T> &vec, T defaultReturn = {})
    {
        GetInstance().m_ret = defaultReturn;

        auto empty = std::queue<T>();
        auto &que = GetInstance().m_queue;
        que.swap(empty);
        for (const T &ele : vec) {
            que.push(ele);
        }
    }
    static void PushReturn(T ret)
    {
        GetInstance().m_queue.push(ret);
    }
    static T PopReturn()
    {
        if (GetInstance().m_queue.empty()) {
            return GetInstance().m_ret;
        }
        T ret = GetInstance().m_queue.front();
        GetInstance().m_queue.pop();
        return ret;
    }

private:
    MockFunc() = default;
    ~MockFunc() = default;
    static MockFunc& GetInstance()
    {
        static MockFunc instance;
        return instance;
    }
    T m_ret {};
    std::queue<T> m_queue {};
};
}
#else
#include <stdio.h>

#define MOCK_RETURN(retValue, retType, funcName) static retType funcName() {return retValue;}

static void Void(){};

#endif
#endif
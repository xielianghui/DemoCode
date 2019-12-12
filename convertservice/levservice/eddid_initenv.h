/**********
 * @file eddid_initenv.h
 * @brief 封装不同平台下对网络环境的初始化
 * @author xiay
 * @version v1.0.0
 * @date 2019.11.28
 **********/
#pragma once
#include "eddid_common.h"
#include "eddid_singleton.h"


namespace eddid { namespace utility {
class CInitializeEnv : public CSingleTonTemplate<CInitializeEnv>
{
    DEF_FRIEND_CLASS(CInitializeEnv)
public:
    ~CInitializeEnv()
    {
        Release();
    }

public:
    std::int32_t GetResult() {return m_result;}

    bool IsValid() { return m_result == 0;}

    void Initialize()
    {
        if (m_result >= 0) {
            return;
        }
        srand((unsigned int)time(nullptr));
#if defined(WIN32)
        LPWSADATA pData = nullptr;
        // alloc from stock
        pData = (LPWSADATA)_alloca(sizeof(WSADATA));
        m_result = ::WSAStartup(MAKEWORD(2, 2), pData);
#else
        m_result = 0;
#endif
    }

    void Release()
    {
#ifdef WIN32
        if (!IsValid()) {
            return;
        }
        ::WSACleanup();
#endif
        m_result = -1;
    }

protected:
    CInitializeEnv(): m_result(-1) {}
private:
    int32_t m_result;
};

} // end of  namespace utility
} // end of namespace eddid




#pragma once
/***
*** @file  eddid_load_lib.h
*** @brief 封装跨平台信号量
*** @author xiay
*** @version v0.1.1
*** @date   2019-11-22
****/


#include "eddid_common.h"

namespace eddid { namespace utility {
class sem_lock
{
    sem_lock() = delete;
    sem_lock(const sem_lock& rhs) = delete;
    sem_lock& operator=(const sem_lock& rhs) = delete;
public:
    sem_lock(bool is_signed);
    ~sem_lock();

public:
    void wait();
    void notify_one();
    void notify_all();
private:
    std::atomic_flag lock_status_;
    std::atomic_uint waiter_count_;
};

class CSemaphore
{
    CSemaphore() = delete;
    CSemaphore(const CSemaphore& rhs) = delete;
    CSemaphore& operator=(const CSemaphore& rhs) = delete;
public:
    CSemaphore(bool bIsSign = false);
    ~CSemaphore();
public:
    // bool OpenSemaphore();
    bool IsValid() const;
    void WaiteForSemaphore(std::int32_t nWaitTime = INFINITE);
    bool Release();
    void ReleaseAll();

    std::uint32_t getWaitorCount() const;
private:
    semaphore_t semaphore_handle_;
    std::atomic_uint waiter_count_;
};
   
    
} // end of namespace utility
} // end of namespace eddid


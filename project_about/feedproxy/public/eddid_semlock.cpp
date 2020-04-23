#include "eddid_semlock.h"
#include <cassert>
#include <cstring>

eddid::utility::sem_lock::sem_lock(bool is_signed)
    : waiter_count_(0)
{
    if (is_signed)
    {
        lock_status_.test_and_set();
    }
}

eddid::utility::sem_lock::~sem_lock()
{
}

void eddid::utility::sem_lock::wait()
{
    //std::chrono::nanoseconds waitTimeSpin(10);
    waiter_count_.fetch_add(1, std::memory_order_relaxed);
    while (lock_status_.test_and_set(std::memory_order_acquire));
    waiter_count_.fetch_sub(1, std::memory_order_relaxed);
}

void eddid::utility::sem_lock::notify_one()
{
    lock_status_.clear();
}

void eddid::utility::sem_lock::notify_all()
{
    while (waiter_count_.load(std::memory_order_relaxed) != 0)
    {
        lock_status_.clear();
    }
}

/////////////////////////////////////////////////////////////////////////////
////
eddid::utility::CSemaphore::CSemaphore(bool bIsSign /*= false*/)
#if defined(_WIN32) || defined(_WIN64)
    : semaphore_handle_(0)
    , waiter_count_(0)
#else
    : waiter_count_(0)
#endif
    
{
#if defined(_WIN32) || defined(_WIN64)
    semaphore_handle_ = ::CreateSemaphore(nullptr, bIsSign ? 1: 0,  1, nullptr);
#else
    memset(&semaphore_handle_, 0, sizeof(sem_t));
    ::sem_init(&semaphore_handle_, 0, bIsSign ? 1: 0);
#endif
}

eddid::utility::CSemaphore::~CSemaphore()
{
    if (IsValid()) {
#if defined(_WIN32) || defined(_WIN64)
        ::CloseHandle(semaphore_handle_);
        semaphore_handle_ = nullptr;
#else
        (void)::sem_destroy(&semaphore_handle_);
        memset(&semaphore_handle_, 0, sizeof(sem_t));
#endif
    }
}

bool eddid::utility::CSemaphore::IsValid() const
{
#if defined(_WIN32) || defined(_WIN64)
    return semaphore_handle_ != nullptr;
#else
    return true;
#endif
}

void eddid::utility::CSemaphore::WaiteForSemaphore(std::int32_t nWaitTime /*= INFINITE*/)
{
    waiter_count_.fetch_add(1, std::memory_order_acquire);
    if (IsValid()){
#if defined(_WIN32) || defined(_WIN64)
        ::WaitForSingleObject(semaphore_handle_, nWaitTime);
#else
        if (nWaitTime == INFINITE) {
            (void)::sem_wait(&semaphore_handle_);
        } else {
            struct timespec stTimeSpec = {nWaitTime / 1000, nWaitTime % 1000 * 1000000};
            (void)::sem_timedwait(&semaphore_handle_, &stTimeSpec);
        }
#endif
    }
    waiter_count_.fetch_sub(1, std::memory_order_release);
}

bool eddid::utility::CSemaphore::Release()
{
    if (IsValid()) {
#if defined(_WIN32) || defined(_WIN64)
        return ::ReleaseSemaphore(semaphore_handle_, 1, nullptr) == TRUE;
#else
        return ::sem_post(&semaphore_handle_) == 0;
#endif
    }
    return false;
}

void eddid::utility::CSemaphore::ReleaseAll()
{
    while (waiter_count_.load(std::memory_order_relaxed) != 0) {
        Release();
    }
}

std::uint32_t eddid::utility::CSemaphore::getWaitorCount() const
{
    return waiter_count_.load(std::memory_order_relaxed);
}

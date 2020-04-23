#pragma once

#include <inttypes.h>
#include <unordered_map>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <list>
#include <vector>
#include <string.h>
#include <event.h>

namespace eddid {
namespace event_wrap {
    class UnDeleteBuffer
    {
    public:
        void operator()(char* ptr) const
        {
            return;
        }
    };

    class EvLoop
    {
    public:
        using EVBaseIndexMap   = std::unordered_map<event_base*, uint32_t>;
        using EVBaseList       = std::vector<event_base*>;
        using EVTimerEventList = std::vector<event*>;
    public:
        EvLoop(const EvLoop& other) = delete;
        EvLoop& operator=(const EvLoop& other) = delete;
        EvLoop(EvLoop&& other) = delete;
        EvLoop& operator=(EvLoop&& other) = delete;
    public:
        EvLoop();
        ~EvLoop();
    public:
        // 初始化
        int Initialize(uint32_t work_threads = 1);

        // 释放
        void Release();

        // 起动
        int Start();

        // 向一个工作者loop中增加一个事件计数
        void Increment(event_base* base);

        // 向一个工作者loop中减少一个事件计数
        void Decrement(event_base* base);

        // 获取一个工作者loop
        event_base* GetWorkLoop();

    public:
        // 定时器回调
        static void OnTimer(evutil_socket_t fd, short event, void* param);

        // 工作者线程
    protected:
        void WorkThreadFunc(event_base* base);
    private:
        bool             is_initialize_;                            // 初始化标志
        std::atomic_bool is_exit_;                                  // 退出标志
        std::atomic_bool is_start_;                                 // 启动标志 
        uint32_t         work_threads_;                             // 工作者线程数
        EVBaseList       lst_work_event_base_;                      // 工作事件集
        EVBaseIndexMap   mp_work_event_base_;
        EVTimerEventList lst_timer_event_;

        std::vector<std::unique_ptr<std::thread>> arr_work_threads_;
    };

} // end of namespace event_wrap
}

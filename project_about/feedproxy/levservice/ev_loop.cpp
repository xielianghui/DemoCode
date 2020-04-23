#include "ev_loop.h"
#include "eddid_initenv.h"

#define DEFAULT_WORK_THREAD_NUM 1
#define DEFAULT_TIMER_TIMEOUT 1

eddid::event_wrap::EvLoop::EvLoop()
    : is_initialize_(false)
    , is_exit_(false)
    , is_start_(false)
    , work_threads_(DEFAULT_WORK_THREAD_NUM)
{
}

eddid::event_wrap::EvLoop::~EvLoop()
{
    Release();
}

// 初始化
int eddid::event_wrap::EvLoop::Initialize(uint32_t work_threads)
{
    if (is_initialize_) {
        return 0;
    }

    utility::CInitializeEnv::instance()->Initialize();

    work_threads_ = work_threads;
    for (uint32_t i = 0; i < work_threads; ++i) {
        event_base* base = event_base_new();
        if (base == nullptr) {
            return -1;
        }
        lst_work_event_base_.emplace_back(std::forward<event_base*>(base));
        mp_work_event_base_.insert(std::make_pair(base, 0));
    }

    is_initialize_ = true;
    return 0;
}

// 释放
void eddid::event_wrap::EvLoop::Release()
{
    if (is_exit_) {
        return;
    }
    is_exit_ = true;
  
    for (auto it = lst_timer_event_.begin(); it != lst_timer_event_.end(); ++it) {
        event_del(*it);
    }

    for (auto it = lst_work_event_base_.begin(); it != lst_work_event_base_.end(); ++it) {
        event_base_loopbreak(*it);
    }

    for (auto it = lst_timer_event_.begin(); it != lst_timer_event_.end(); ++it) {
        event_free(*it);
    }
    lst_timer_event_.clear();

    for (auto it = lst_work_event_base_.begin(); it != lst_work_event_base_.end(); ++it) {
        event_base_free(*it);
    }
    lst_work_event_base_.clear();

    for (auto it = arr_work_threads_.begin(); it != arr_work_threads_.end(); ++it) {
        (*it)->join();
    }
    arr_work_threads_.clear();
}

// 起动
int eddid::event_wrap::EvLoop::Start()
{
    if (!is_initialize_) {
        return -1;
    }

    timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = DEFAULT_TIMER_TIMEOUT;
    for (uint32_t i = 0; i < work_threads_; ++i) {
        event_base* base = lst_work_event_base_[i];
        event* timer = event_new(base, -1, EV_PERSIST, OnTimer, event_self_cbarg());
        if (timer == nullptr) {
            return -1;
        }
        event_add(timer, &tv);
        lst_timer_event_.emplace_back(std::forward<event*>(timer));
        arr_work_threads_.emplace_back(std::make_unique<std::thread>(&event_wrap::EvLoop::WorkThreadFunc, this, base));
    }

    is_start_ = true;
    return 0;
}

// 向一个工作者loop中增加一个事件计数
void eddid::event_wrap::EvLoop::Increment(event_base* base)
{
    if (base == nullptr) {
        return;
    }

    auto item = mp_work_event_base_.find(base);
    if (item != mp_work_event_base_.end()) {
        item->second++;
    }
}

// 向一个工作者loop中减少一个事件计数
void eddid::event_wrap::EvLoop::Decrement(event_base* base)
{
    if (base == nullptr) {
        return;
    }

    auto item = mp_work_event_base_.find(base);
    if (item != mp_work_event_base_.end()) {
        item->second--;
    }
}

// 获取一个工作者loop
event_base* eddid::event_wrap::EvLoop::GetWorkLoop()
{
    uint32_t cnt(0);
    uint32_t find_index(0);
    event_base *base = lst_work_event_base_[find_index];
    auto item = mp_work_event_base_.find(base);
    cnt = item->second;
    for (uint32_t index = 0; index < lst_work_event_base_.size(); ++index) {
        base = lst_work_event_base_[index];
        auto it = mp_work_event_base_.find(base);
        if (it->second < cnt) {
            cnt = it->second;
            find_index = index;
        }
    }

    return lst_work_event_base_[find_index];
}

void eddid::event_wrap::EvLoop::WorkThreadFunc(event_base* base)
{
    if (base == nullptr) {
        return;
    }
    event_base_dispatch(base);
    is_start_ = false;

    printf("event_wrap::EvLoop::WorkThreadFunc exit\n");
}

void eddid::event_wrap::EvLoop::OnTimer(evutil_socket_t fd, short event, void* param)
{
    // nothing
}

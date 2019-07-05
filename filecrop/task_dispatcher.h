#pragma once
#include <atomic>
#include <vector>
#include <mutex>

#include <uv.h>

#include <base_task.h>

class TaskDispatcher final
{
public:
    TaskDispatcher() = delete;
    ~TaskDispatcher() = delete;
    static int Dispatch(BaseTask* pTask, uv_loop_t* loop);
    static std::vector<std::pair<char*, int>>& GetResult();
    static void ClearResult();

private:
    static std::mutex m_mtx;
    static void startWork(uv_work_t* req);
    static void afterWork(uv_work_t* req, int status);
    static std::atomic<int> m_taskRunningCounts;
    static std::vector<std::pair<char*, int>> m_fileDataVec;
};

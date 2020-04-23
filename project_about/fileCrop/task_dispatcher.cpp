#include "task_dispatcher.h"

std::mutex TaskDispatcher::m_mtx;
std::atomic<int> TaskDispatcher::m_taskRunningCounts;
std::vector<std::pair<char*, int>> TaskDispatcher::m_fileDataVec;

int TaskDispatcher::Dispatch(BaseTask* pTask, uv_loop_t* loop)
{
    int res = 0;
    if (pTask != nullptr){ 
        uv_work_t* pReq = new(std::nothrow) uv_work_t();
        if (pReq != nullptr) {
            pReq->data = (void*)pTask;
            res = uv_queue_work(loop, pReq, &TaskDispatcher::startWork, &TaskDispatcher::afterWork);
        }
        else {
            res = -1;
        }
    }
    else {
        res = -1;
    }
    return res;
}

void TaskDispatcher::startWork(uv_work_t* req)
{
    if ((req != nullptr) && (req->data != nullptr)){
        ++m_taskRunningCounts;
        BaseTask* pTask = (BaseTask*)req->data;
        pTask->Run();
    }
}

void TaskDispatcher::afterWork(uv_work_t *req, int status)
{
    if ((req != nullptr) && (req->data != nullptr)){
        BaseTask* pTask = (BaseTask*)req->data;
        char* pStart = nullptr;
        int size = 0;
        pTask->EndRun(pStart, size);

        m_mtx.lock();
        m_fileDataVec.emplace_back(std::make_pair(pStart, size));
        m_mtx.unlock();

        delete req;
        req = nullptr;
        --m_taskRunningCounts;
    }
}

std::vector<std::pair<char*, int>>& TaskDispatcher::GetResult()
{
    return m_fileDataVec;
}

void TaskDispatcher::ClearResult()
{
    m_fileDataVec.clear();
}
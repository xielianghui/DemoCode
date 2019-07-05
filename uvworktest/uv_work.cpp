#include <iostream>
#include <thread>
#include <unistd.h>
#include <uv.h>

using namespace std;

class UvWork {
public:
    UvWork() = default;
    UvWork(uv_loop_t * uvloop) :m_loop(uvloop) { 
        m_data = new int[3];
        m_data[0] = 1;
        m_data[1] = 2;
        m_data[2] = 3;
    }
    ~UvWork();
    void DoWork();
private:
    static void StartTaskWork(uv_work_t *req);
    static void AfterTaskWork(uv_work_t *req, int status);

private:
    uv_loop_t * m_loop;
    int * m_data;
};

UvWork::~UvWork()
{
    delete m_data;
}

void UvWork::StartTaskWork(uv_work_t *req)
{
    if (req != nullptr && req->data != nullptr) {
        int * data = (int *)req->data;
        sleep(*data);
        cout << "This is " << *data << "th thread and sleep " << *data << " second" << std::endl;
    }
}

void UvWork::AfterTaskWork(uv_work_t *req, int status)
{
    if (req != nullptr) {
        if (req->data != nullptr) {
            cout << *((int *)(req->data)) << "th task is down" << std::endl;
        }
        delete req; req = nullptr;
    }
    
}

void UvWork::DoWork()
{
    for (int i = 0; i < 3; i++)
    {
        uv_work_t * preq = new(std::nothrow) uv_work_t();
        preq->data = (void *)&m_data[i];
        uv_queue_work(m_loop, preq, &UvWork::StartTaskWork, &UvWork::AfterTaskWork);
        cout << "Do work num:" << i + 1 << endl;
    }

}


int main()
{

    uv_loop_t * loop = uv_default_loop();
    UvWork uvWork(loop);
    uvWork.DoWork();
    return uv_run(loop, UV_RUN_DEFAULT);
}

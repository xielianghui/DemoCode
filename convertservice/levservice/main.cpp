#include <iostream>
#include "service.h"

class CBHandle final
{
public:
    using CContext = typename eddid::event_wrap::Service<CBHandle>::CContext;
    CBHandle() = default;
    ~CBHandle() = default;

public:
    void OnAccept(CContext* conn, const char* peer, uint32_t len);       // 接受连接回调
    void OnReadDone(CContext* conn, evbuffer*&& recv_data);              // 读完成回调
    void OnDisConnect(CContext* conn);                                   // 断线回调
    void OnError(CContext* conn, int err, const char* err_msg);          // 错误消息回调
};

void CBHandle::OnAccept(CContext *conn, const char *peer, uint32_t len)
{
    std::cout<<"OnAccept()"<<std::endl;
}

void CBHandle::OnReadDone(CContext *conn, evbuffer *&&recv_data)
{
    std::cout<<"OnReadDone()"<<std::endl;
}

void CBHandle::OnDisConnect(CContext *conn)
{
    std::cout<<"OnDisConnect()"<<std::endl;
}

void CBHandle::OnError(CContext *conn, int err, const char *err_msg)
{
    std::cout<<"OnError()"<<std::endl;
}


int main()
{
    CBHandle cbHdl;
    eddid::event_wrap::EvLoop loopHdl;
    if (loopHdl.Initialize() != 0) {
        std::cout<<"loop init failed"<<std::endl;
        return -1;
    }
    if (loopHdl.Start() != 0) {
        loopHdl.Release();
        std::cout<<"loop start failed"<<std::endl;
        return 0;
    }

    eddid::event_wrap::Service<CBHandle> service(&loopHdl, cbHdl);
    service.Initialize("127.0.0.1", 8032);
    service.Start();
    char ch_input;
    while (true) {
        std::cin >> ch_input;
        if (ch_input == 'q' || ch_input == 'Q') {
            break;
        }
    }
    service.Release();
    loopHdl.Release();
    return 0;
}


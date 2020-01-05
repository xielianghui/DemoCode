#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

#include <string>

#include <event.h>

#include "common_hdr.h"

#define READ 0
#define WRITE 1
#define ONE_RECV_SIZE 2048

static int fds[2]{-1, -1};
static char recvBuf[ONE_RECV_SIZE]{0};
static std::string recvMsg;

template<typename T>
class EventNotify
{
    public:
        typedef void(T::*OnEventNotify)(std::string&&);

        EventNotify() = delete;
        EventNotify(event_base* pEvBase, T* pService, OnEventNotify notify):
            m_eventBase(pEvBase),
            m_mainService(pService),
            m_notify(notify),
            m_ev(nullptr){}

        ~EventNotify()
        {
            if(m_ev){
                event_free(m_ev);
            }
            close(fds[0]);
            close(fds[1]);
        }

        int Init()
        {
            // create pipe 
            int ret = pipe(fds);
            if(ret == -1){
                std::cout<<"pipe() failed, error msg: "<<strerror(errno)<<std::endl;
                return ret;
            }
            ret = fcntl(fds[READ], F_SETFL, fcntl(fds[READ],F_GETFL) | O_NONBLOCK);
            if(ret == -1){
                std::cout<<"fcntl() failed"<<std::endl;
                return ret;
            }
            ret = fcntl(fds[WRITE], F_SETFL, fcntl(fds[WRITE],F_GETFL) | O_NONBLOCK);
            if(ret == -1){
                std::cout<<"fcntl() failed"<<std::endl;
                return ret;
            }
            // create event
            m_ev = event_new(m_eventBase, fds[READ], EV_READ | EV_PERSIST, OnRecv, this);
            if(m_ev == nullptr){
                std::cout<<"event_new() failed"<<std::endl;
                return -1;
            }
            timeval tv;
            evutil_timerclear(&tv);
            tv.tv_usec = 200000;// 200ms
            event_add(m_ev, &tv);
            return 0;
        }

        void Notify(const std::string& sendMsg)
        {
            std::string data;
            data.resize(sendMsg.size() + eddid::COMMON_HDR_LEN);
            eddid::ST_COMMONHDR* pHdr = (eddid::ST_COMMONHDR*)data.data();
            new (pHdr) eddid::ST_COMMONHDR(0, sendMsg.size());
            memcpy(data.data() + eddid::COMMON_HDR_LEN, sendMsg.data(), sendMsg.size());
			write(fds[WRITE], data.c_str(), data.size());
        }

    private:
        static void OnRecv(evutil_socket_t fd, short event, void* args)
        {
            int len{0};
            while((len = read(fds[READ], recvBuf, ONE_RECV_SIZE)) > 0)
            {
                recvMsg.append(std::string(recvBuf, len));
            }
            while(true)
            {
                char hdl[eddid::COMMON_HDR_LEN];
                memcpy(hdl, recvMsg.data(), eddid::COMMON_HDR_LEN);
                eddid::ST_COMMONHDR *pHdl = (eddid::ST_COMMONHDR *)hdl;
                int dataLen = pHdl->data_len_;
                if (dataLen + eddid::COMMON_HDR_LEN > recvMsg.size()){
                    break;
                }
                else{
                    std::string realMsg(recvMsg.data() + eddid::COMMON_HDR_LEN, dataLen);
                    EventNotify<T> *pThis = reinterpret_cast<EventNotify<T> *>(args);
                    if (pThis){
                        OnEventNotify cb = pThis->m_notify;
                        (pThis->m_mainService->*cb)(std::move(realMsg));
                    }
                    recvMsg = recvMsg.substr(eddid::COMMON_HDR_LEN + dataLen);
                    if(recvMsg.size() < eddid::COMMON_HDR_LEN) break;
                }
            }
        }
    public:
        T* m_mainService;
        OnEventNotify m_notify;
        event_base* m_eventBase;
        event* m_ev;
        
};
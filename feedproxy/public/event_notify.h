#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

#include <string>

#include <event.h>

#define READ 0
#define WRITE 1

static int fds[2]{-1, -1};
static std::string defaultMsg = "#";
template<typename T>
class EventNotify
{
    public:
        typedef void(T::*OnEventNotify)();

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

        void Notify()
        {
			write(fds[WRITE], defaultMsg.c_str(), defaultMsg.size());
        }

    private:
        static void OnRecv(evutil_socket_t fd, short event, void* args)
        {
            char chr;
            int ret = read(fds[READ], &chr, 1);
            if(ret == 1){
                EventNotify<T> *pThis = reinterpret_cast<EventNotify<T> *>(args);
                if (pThis){
                    OnEventNotify cb = pThis->m_notify;
                    (pThis->m_mainService->*cb)();
                }
            }
        }
    public:
        T* m_mainService;
        OnEventNotify m_notify;
        event_base* m_eventBase;
        event* m_ev;
        
};
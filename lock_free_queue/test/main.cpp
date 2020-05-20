#include <string>
#include <thread>
#include <chrono>

#include "../lock_free_queue.hpp"

RoughLockFreeQueue<int> lfq;

void enFunc1()
{
    for(int i = 0; i < 100; ++i)
    {
        int* num = new int(i);
        lfq.EnQueue(num);
    }
}

void enFunc2()
{
    for(int i = 100; i < 200; ++i)
    {
        int* num = new int(i);
        lfq.EnQueue(num);
    }
}

void deFunc()
{
    while(true)
    {
        if(!lfq.empty()){
            int* num = lfq.DeQueue();
            if(num != nullptr){
                std::cout<< *num <<std::endl;
                delete num;
            }else{
                std::cout<< "get empty" <<std::endl;
            }
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 1));
    }
}

int main()
{
    std::thread wth1(&enFunc1);
    std::thread wth2(&enFunc2);

    std::thread rth(&deFunc);

    wth1.join();
    wth2.join();
    rth.join();

    return 0;
}
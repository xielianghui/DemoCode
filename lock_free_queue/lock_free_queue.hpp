#include <iostream>
#include <atomic>

template<typename T>
struct QueueNode
{
    T* data;
    std::atomic<QueueNode<T>*> next;
    QueueNode():data(nullptr), next(nullptr){}
};

template<typename T>
class RoughLockFreeQueue
{
public:
    RoughLockFreeQueue():
        m_head(new QueueNode<T>()),
        m_tail(m_head.load(std::memory_order_relaxed)),
        m_size(0){}

    ~RoughLockFreeQueue()
    {

    }

    RoughLockFreeQueue(const RoughLockFreeQueue &) = delete;
    RoughLockFreeQueue(RoughLockFreeQueue &&) = delete;
    RoughLockFreeQueue &operator=(const RoughLockFreeQueue &) = delete;
    RoughLockFreeQueue &operator=(RoughLockFreeQueue &&) = delete;

public:
    void EnQueue(T* t)
    {
        auto newNode = new QueueNode<T>();
        newNode->data = t;

        QueueNode<T>* tail;
        QueueNode<T>* expecTtail = nullptr;
        // CAS loop
        do{
            expecTtail = nullptr;
            tail = m_tail;
        } while(!tail->next.compare_exchange_weak(expecTtail, newNode));

        m_tail.compare_exchange_weak(tail, newNode); //不需要loop
        m_size.fetch_add(1, std::memory_order_relaxed);
    }

    T* DeQueue()
    {
        QueueNode<T>* head;
        do{
            head = m_head;
            if(head->next == nullptr){
                return nullptr;
            }

        }while(!m_head.compare_exchange_weak(head, head->next));

        m_size.fetch_sub(1, std::memory_order_relaxed);
        T* res = head->data;
        delete head;
        return res;
    }

    bool empty()
    {
        return (m_size.load(std::memory_order_relaxed) == 0);
    }

private:
    std::atomic<QueueNode<T>*> m_head;
    std::atomic<QueueNode<T>*> m_tail;
    std::atomic<uint32_t> m_size;
};
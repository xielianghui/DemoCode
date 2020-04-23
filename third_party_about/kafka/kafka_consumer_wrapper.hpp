#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <memory>

#include "event.h"
#include "librdkafka/rdkafkacpp.h"

#include "../utils/eddid_functional.h"
#include "../utils/concurrentqueue.h"

namespace eddid{
namespace kafkautils{

class CKafkaConsumerWrapper
{
    using OnMsgReply = eddid_function_wrap<void(std::string&&, std::string&&)>;
    
public:
    CKafkaConsumerWrapper(event_base* base = nullptr):
        m_base(base),
        m_isOuterBase(true),
        m_timerEvent(nullptr),
        m_consumerPtr(nullptr),
        m_confPtr(nullptr){}

    ~CKafkaConsumerWrapper()
    {
        if(m_timerEvent){
            event_free(m_timerEvent);
        }
        if(!m_isOuterBase){
            event_base_free(m_base);
            if(m_th.joinable()){
                m_th.join();
            }
        }
        // kafka release
        m_consumerPtr->close();
    }

public:
    /**
     * \brief init producer
     * \param host kafka server host, e.g. localhost:9092,localhost:9093,localhost:9094
     * \param groupId All consumers sharing the same group id will join the same group
     * \param conf kafka config
     * \return returns 0 if successful. Otherwise returns -1
     */
    int Init(const std::string& host, const std::string groupId, RdKafka::Conf* conf = nullptr)
    {
        std::string errStr;
        // kafka conf init
        if(conf == nullptr){
            conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
        }
        std::shared_ptr<RdKafka::Conf> comfigTempPtr(conf);
        m_confPtr.swap(comfigTempPtr);

        if(m_confPtr->set("bootstrap.servers", host, errStr) != RdKafka::Conf::CONF_OK) {
            std::cerr << errStr << std::endl;
            return -1;
        }
        if(m_confPtr->set("group.id", groupId, errStr) != RdKafka::Conf::CONF_OK) {
            std::cerr << errStr << std::endl;
            return -1;
        }
        // if(m_confPtr->set("consume_cb", this, errStr) != RdKafka::Conf::CONF_OK) {
        //     std::cerr << errStr << std::endl;
        //     return -1;
        // }

        // kafka handl init
        std::shared_ptr<RdKafka::KafkaConsumer> consumerTempPtr(RdKafka::KafkaConsumer::create(conf, errStr));
        m_consumerPtr.swap(consumerTempPtr);
        if(m_consumerPtr == nullptr){
            std::cerr << "Failed to create kafka comsumer, error msg: " << errStr <<std::endl;
            return -1;
        }

        if(m_base == nullptr){
            m_base = event_base_new();
            m_isOuterBase = false;
        }
        // create event fd
        timeval tv{0, 0};
        tv.tv_usec = 1000 * 50;
        m_timerEvent = event_new(m_base, -1, EV_PERSIST, OnTimer, this);
        if(m_timerEvent == nullptr){
            std::cerr<<"event_new() failed"<<std::endl;
            return -1;
        }
        event_add(m_timerEvent, &tv);
        if(!m_isOuterBase){
            m_th = std::thread(&CKafkaConsumerWrapper::DoPoll, this);
        }
        return 0;
    }

    /**
     * \brief producer messgae to kafka
     * \param topic topic for message
     * \return returns 0 if successful. Otherwise returns errno
     */
    int Subscribe(const std::string& topic)
    {
        std::vector<std::string> topics{topic};
        return Subscribe(topics);
    }

    /**
     * \brief producer messgae to kafka
     * \param topics topic for message
     * \return returns 0 if successful. Otherwise returns errno
     */
    int Subscribe(const std::vector<std::string>& topics)
    {
        if(m_consumerPtr != nullptr){
            RdKafka::ErrorCode err = m_consumerPtr->subscribe(topics);
            if (err){
                std::cerr << "Failed to subscribe to " << topics.size() << " topics: "
                          << RdKafka::err2str(err) << std::endl;
                return -1;
            }
        }
        return -1;
    }

    void RegisterCallback(OnMsgReply cb)
    {
        m_cb = cb;
    }

public:
    void ConsumeCb(RdKafka::Message &msg, void *opaque)
    {
        if(m_cb)
        {
            if(!msg.err()){
                (m_cb)(msg.topic_name(), std::string((char*)msg.payload(), msg.len()));
            }
            else{
                if(msg.err() == RdKafka::ErrorCode::ERR__TIMED_OUT){

                }
                else{
                    std::cerr << "Consume failed: " << msg.errstr() << std::endl;
                }
            }
        }
    }

private:
    static void OnTimer(evutil_socket_t fd, short event, void *args)
    {
        CKafkaConsumerWrapper* pThis = static_cast<CKafkaConsumerWrapper*>(args);
        if (pThis){
            auto msg =pThis->m_consumerPtr->consume(0);
            pThis->ConsumeCb(*msg, nullptr);
            delete msg;
        }
    }

    void DoPoll()
    {
        event_base_dispatch(m_base);
    }


private:
    event_base* m_base;
    OnMsgReply m_cb;
    std::thread m_th;
    bool m_isOuterBase;
    event* m_timerEvent;

    std::shared_ptr<RdKafka::KafkaConsumer> m_consumerPtr;
    std::shared_ptr<RdKafka::Conf> m_confPtr;

};

}
}
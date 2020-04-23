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

class CKafkaProducerWrapper : public RdKafka::DeliveryReportCb
{
    using OnDeliveryReport = eddid_function_wrap<void(void*, RdKafka::ErrorCode, std::string&&)>;

public:
    CKafkaProducerWrapper(event_base* base = nullptr):
        m_base(base),
        m_isOuterBase(true),
        m_timerEvent(nullptr),
        m_producerPtr(nullptr),
        m_confPtr(nullptr){}

    ~CKafkaProducerWrapper()
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
        m_producerPtr->flush(3 * 1000);
        if (m_producerPtr->outq_len() > 0){
            std::cerr << m_producerPtr->outq_len() << " message(s) were not delivered" << std::endl;
        }
        RdKafka::wait_destroyed(3* 1000);
    }

public:
    /**
     * \brief init producer
     * \param host kafka server host, e.g. localhost:9092,localhost:9093,localhost:9094
     * \param conf kafka config, free internal
     * \return returns 0 if successful. Otherwise returns -1
     */
    int Init(const std::string& host, RdKafka::Conf* conf = nullptr)
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
        if(m_confPtr->set("dr_cb", this, errStr) != RdKafka::Conf::CONF_OK) {
            std::cerr << errStr << std::endl;
            return -1;
        }
        // kafka handl init
        std::shared_ptr<RdKafka::Producer> producerTempPtr(RdKafka::Producer::create(conf, errStr));
        m_producerPtr.swap(producerTempPtr);
        if(m_producerPtr == nullptr){
            std::cerr << "Failed to create kafka producer, error msg: " << errStr <<std::endl;
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
            m_th = std::thread(&CKafkaProducerWrapper::DoPoll, this);
        }
        return 0;
    }

    /**
     * \brief producer messgae to kafka
     * \param data user's data
     * \param topic topic for message
     * \param msg real message
     * \return returns 0 if successful. Otherwise returns errno
     */
    int ProducerMsg(void* data, const std::string& topic, const std::string& msg)
    {
        if(m_producerPtr != nullptr){
            RdKafka::ErrorCode err = m_producerPtr->produce( /* Topic name */
                                                        topic,
                                                        /* Any Partition: the builtin partitioner will be
                                                         * used to assign the message to a topic based
                                                         * on the message key, or random partition if
                                                         * the key is not set. */
                                                        RdKafka::Topic::PARTITION_UA,
                                                        /* Make a copy of the value */
                                                        0x0,
                                                        /* Value */
                                                        const_cast<char *>(msg.c_str()), msg.size(),
                                                        /* Key */
                                                        NULL, 0,
                                                        /* Timestamp (defaults to current time) */
                                                        0,
                                                        /* Message headers, if any */
                                                        NULL,
                                                        /* Per-message opaque value passed to
                                                         * delivery report */
                                                        data);
        
            if(err != RdKafka::ERR_NO_ERROR) {
                std::cerr << "Failed to produce to topic: " << topic << ", error msg:" << RdKafka::err2str(err) << std::endl;
                return err;
            }
            else{
                std::cout << "Enqueued message, bytes: " << msg.length() << ", topic: " << topic << std::endl;
            }
            return 0;
        }
        return -1;
    }

    void RegisterCallback(OnDeliveryReport cb)
    {
        m_cb = cb;
    }

public:
    void dr_cb(RdKafka::Message &message)
    {
        if(m_cb){
            if (message.err()){
                std::cerr << "Message delivery failed: " << message.errstr() << std::endl;
                (m_cb)(message.msg_opaque(), message.err(), message.errstr());
            }
            else{
                uint64_t len = message.len();
                (m_cb)(message.msg_opaque(), RdKafka::ErrorCode::ERR_NO_ERROR, std::string((char*)message.payload(), message.len()));
            }
        }
    }

private:
    static void OnTimer(evutil_socket_t fd, short event, void *args)
    {
        CKafkaProducerWrapper* pThis = static_cast<CKafkaProducerWrapper*>(args);
        if (pThis){
            pThis->m_producerPtr->poll(0);
        }
    }

    void DoPoll()
    {
        event_base_dispatch(m_base);
    }


private:
    event_base* m_base;
    OnDeliveryReport m_cb;
    std::thread m_th;
    bool m_isOuterBase;
    event* m_timerEvent;

    std::shared_ptr<RdKafka::Producer> m_producerPtr;
    std::shared_ptr<RdKafka::Conf> m_confPtr;

};

}
}
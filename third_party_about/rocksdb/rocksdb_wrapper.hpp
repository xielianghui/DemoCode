#pragma once
#include <sys/eventfd.h>

#include <event.h>
#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#include <rocksdb/options.h>
#include <rocksdb/iterator.h>

#include "../utils/eddid_common.h"
#include "../utils/eddid_functional.h"
#include "../utils/concurrentqueue.h"
#include "../utils/eddid_semlock.h"

namespace eddid{
namespace rocksdbutils{

enum class EN_CMD_TYPE
{
    GET = 0,
    SEEK = 1,
};

struct ST_GetCommand
{
    void* data;
    std::vector<std::string> keys;
    rocksdb::ReadOptions readOptions;
    EN_CMD_TYPE cmdType;
    ST_GetCommand(){}
    ST_GetCommand(void* d, const std::vector<std::string>& k, const rocksdb::ReadOptions& r, const EN_CMD_TYPE& t = EN_CMD_TYPE::GET):
        data(d),
        keys(k),
        readOptions(r),
        cmdType(t){}
};

struct ST_GetResult
{
    void* data;
    std::vector<std::string> values;
};

class CRocksdbWrapper
{
    using OnGetCallback = eddid_function_wrap<void(void*, std::vector<std::string>&)>;

public:
    CRocksdbWrapper(event_base* pEvBase = nullptr):
        m_isOuterBase(true),
        m_base(pEvBase),
        m_pDB(nullptr),
        m_isOpen(false),
        m_sem(false),
        m_eventFd(-1),
        m_notifyEvent(nullptr){}

    ~CRocksdbWrapper()
    {
        m_isOpen = false;
        m_sem.ReleaseAll();
        if(m_cmdThread.joinable()){
            m_cmdThread.join();
        }

        if(m_notifyEvent){
            event_free(m_notifyEvent);
        }
        if(!m_isOuterBase){
            event_base_free(m_base);
            if(m_baseThread.joinable()){
                m_baseThread.join();
            }
        }

        if(m_eventFd != -1){
            close(m_eventFd);
        }

        if(m_pDB != nullptr){
            rocksdb::Status s = m_pDB->Close();
            if(!s.ok()){
                std::cerr << "Close database failed, error msg: " << s.ToString() <<std::endl;
            }
            delete m_pDB;
        }
    }

    /**
     * \param dbPath all rocksdb file will save in this folder.
     * \param options Optimize RocksDB. some options can be changed dynamically while DB is running.
     * \return 0 success -1 failure
     */
    int Init(const std::string& dbPath, rocksdb::Options options = rocksdb::Options())
    {
        m_dbPath = dbPath;
        // open database with options
        options.create_if_missing = true;// create the DB if it's not already present
        m_dbOptions = options;
        rocksdb::Status s = rocksdb::DB::Open(options, dbPath, &m_pDB);
        if(!s.ok()){
            std::cerr << "Open database failed, error msg :" << s.ToString() << std::endl;
            return -1;
        }
        m_isOpen = true;
        if(m_base == nullptr){
            m_isOuterBase = false;
            m_base = event_base_new();
        }
        // create event fd
        m_eventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (m_eventFd < 0){
            std::cerr << "eventfd() failed, error msg: " << strerror(errno) << std::endl;
            return -1;
        }
        m_notifyEvent = event_new(m_base, m_eventFd, EV_READ | EV_PERSIST, OnNotify, this);
        if(m_notifyEvent == nullptr){
            std::cerr<<"event_new() failed"<<std::endl;
            return -1;
        }
        event_add(m_notifyEvent, nullptr);

        // start do thread
        m_cmdThread = std::thread(&CRocksdbWrapper::DoGet, this);
        if(m_isOuterBase){
            m_baseThread = std::thread(&CRocksdbWrapper::DoRun, this);
        }
        return 0;
    }

    /**
     * atomic write data to rocksDB, batch writes are generally more efficient than single
     * \param key_or_keys
     * \param value_or_values
     * \param wrireOptions write options.
     * \return 0 success -1 failure
     */
    int Put(const std::string& key, const std::string& value, const rocksdb::WriteOptions& wrireOptions = rocksdb::WriteOptions())
    {
        if(m_pDB == nullptr) return -1;
        rocksdb::Status s = m_pDB->Put(wrireOptions, key, value);
        if (!s.ok()){
            std::cerr << "Put failed, key: " << key << ", value: " << value << ", error msg: " << s.ToString() << std::endl;
            return -1;
        }
        return 0;
    }

    int Put(const std::vector<std::string>& keys, const std::vector<std::string>& values, const rocksdb::WriteOptions& wrireOptions = rocksdb::WriteOptions())
    {
        if(m_pDB == nullptr || keys.size() != values.size()) return -1;
        if(keys.empty()) return 0;
        rocksdb::WriteBatch batch;
        for(int i = 0; i < keys.size(); ++i)
        {
            batch.Put(keys[i], values[i]);
        }
        rocksdb::Status s = m_pDB->Write(wrireOptions, &batch);
        if(!s.ok()){
            std::cerr << "Batch put failed, error msg: " << s.ToString() <<std::endl;
            return -1;
        }
        return 0;
    }

    /**
     * \param data user's data
     * \param keys_or_key you want get values's key 
     * \param readOptions read options
     * \return 0 success -1 failure, callback function res string will be empty
     */
    int Get(void* data, const std::vector<std::string>& keys, const rocksdb::ReadOptions& readOptions = rocksdb::ReadOptions())
    {
        ST_GetCommand getCmd(data, keys, readOptions);
        m_cmdQueue.enqueue(std::forward<ST_GetCommand>(getCmd));
        m_sem.Release();
        return 0;
    }

    int Get(void* data, const std::string& key, const rocksdb::ReadOptions& readOptions = rocksdb::ReadOptions())
    {
        std::vector<std::string> reqVec;
        reqVec.emplace_back(key);
        return Get(data, reqVec, readOptions);
    }

    /**
     * \param key_or_keys
     * \param wrireOptions write options
     * \return 0 success -1 failure
     */
    int Delete(const std::string& key, const rocksdb::WriteOptions& wrireOptions = rocksdb::WriteOptions())
    {
        if(m_pDB == nullptr) return -1;
        rocksdb::Status s = m_pDB->Delete(wrireOptions, rocksdb::Slice(key));
        if (!s.ok()){
            std::cerr << "Delete failed, keys: " << key << ", error msg: " << s.ToString() << std::endl;
            return -1;
        }
        return 0;
    }

    int Delete(const std::vector<std::string>& keys, const rocksdb::WriteOptions& wrireOptions = rocksdb::WriteOptions())
    {
        if(m_pDB == nullptr) return -1;
        rocksdb::WriteBatch batch;
        for(int i = 0; i < keys.size(); ++i)
        {
            batch.Delete(keys[i]);
        }
        rocksdb::Status s = m_pDB->Write(wrireOptions, &batch);
        if(!s.ok()){
            std::cerr << "Delete failed, error msg: " << s.ToString() << std::endl;
            return -1;
        }
        return 0;
    }

    /**
     * \param data user's data
     * \param start start pos, if empty will start first
     * \param end end pos, if empty will end last
     * \param readOptions read options
     * \return 0 success -1 failure,call back will return [start, end) values
     */
    int Seek(void* data, const std::string& start, const std::string& end, const rocksdb::ReadOptions& readOptions = rocksdb::ReadOptions())
    {
        std::vector<std::string> reqVec;
        reqVec.emplace_back(start);
        reqVec.emplace_back(end);
        ST_GetCommand seekCmd(data, reqVec, readOptions, EN_CMD_TYPE::SEEK);
        m_cmdQueue.enqueue(std::forward<ST_GetCommand>(seekCmd));
        m_sem.Release();
        return 0;
    }

    void RegisterCallback(const OnGetCallback& cb)
    {
        m_cb = cb;
    }

private:
    static void OnNotify(evutil_socket_t fd, short event, void *args)
    {
        CRocksdbWrapper* pThis = static_cast<CRocksdbWrapper*>(args);
        if (pThis && pThis->m_cb){
            uint64_t u;
            read(pThis->m_eventFd, &u, sizeof(uint64_t));
            ST_GetResult getRes;
            while(pThis->m_resQueue.try_dequeue(getRes)){
                (pThis->m_cb)(getRes.data, getRes.values);
            }
        }
        
    }

    void DoGet()
    {
        while(true)
        {
            ST_GetCommand getCmd;
            while(!m_cmdQueue.try_dequeue(getCmd) && m_isOpen)
            {
                m_sem.WaiteForSemaphore();
            }
            if(!m_isOpen) break;
            if (m_pDB){
                ST_GetResult getRes;
                getRes.data = getCmd.data;
                if(getCmd.cmdType == EN_CMD_TYPE::GET){
                    std::vector<rocksdb::Slice> keys;
                    keys.reserve(getCmd.keys.size());
                    for (auto &it : getCmd.keys)
                    {
                        keys.emplace_back(rocksdb::Slice(it));
                    }
                    m_pDB->MultiGet(getCmd.readOptions, keys, &(getRes.values));
                }
                else if(getCmd.cmdType == EN_CMD_TYPE::SEEK){
                    rocksdb::Iterator* start = m_pDB->NewIterator(getCmd.readOptions);
                    if(getCmd.keys.size() < 2) continue;
                    rocksdb::Slice endSlice(getCmd.keys[1]);
                    for(getCmd.keys[0].empty() ? start->SeekToFirst() : start->Seek(getCmd.keys[0]);
                        start->Valid() && (endSlice.empty() || m_dbOptions.comparator->Compare(start->key(), endSlice) < 0);
                        start->Next()){
                            getRes.values.emplace_back(start->value().ToString());
                    }
                    delete start;
                }
                m_resQueue.enqueue(std::forward<ST_GetResult>(getRes));
                uint64_t u = 1;
                write(m_eventFd, &u, sizeof(uint64_t));
            }
        }
    }

    void DoRun()
    {
        event_base_dispatch(m_base);
    }

private:
    bool m_isOuterBase;
    event_base* m_base;

    OnGetCallback m_cb;
    std::string m_dbPath;
    rocksdb::DB* m_pDB;
    std::thread m_cmdThread;
    std::thread m_baseThread;
    std::atomic<bool> m_isOpen;
    moodycamel::ConcurrentQueue<ST_GetCommand> m_cmdQueue;
    moodycamel::ConcurrentQueue<ST_GetResult> m_resQueue;
    utility::CSemaphore m_sem;
    rocksdb::Options m_dbOptions;

    int m_eventFd;
    event* m_notifyEvent;
};

}
}


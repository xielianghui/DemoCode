#pragma once

#include <event.h>
#include <zookeeper/zookeeper.h>

#include "../utils/eddid_common.h"
#include "../utils/eddid_functional.h"

namespace eddid{
namespace zkutils{

namespace inner{
inline const char *type2string(int type)
{
    if (type == ZOO_CREATED_EVENT){
        return "ZOO_CREATED_EVENT";
    }
    else if (type == ZOO_DELETED_EVENT){
        return "ZOO_DELETED_EVENT";
    }
    else if (type == ZOO_CHANGED_EVENT){
        return "ZOO_CHANGED_EVENT";
    }
    else if (type == ZOO_CHILD_EVENT){
        return "ZOO_CHILD_EVENT";
    }
    else if (type == ZOO_SESSION_EVENT){
        return "ZOO_SESSION_EVENT";
    }
    else if (type == ZOO_NOTWATCHING_EVENT){
        return "ZOO_NOTWATCHING_EVENT";
    }
    else{
        return "UNKNOWN_ZOO_TYPE";
    }
}

inline const char *state2string(int state)
{
    if(state == ZOO_EXPIRED_SESSION_STATE){
        return "ZOO_EXPIRED_SESSION_STATE";
    }
    else if(state == ZOO_AUTH_FAILED_STATE){
        return "ZOO_AUTH_FAILED_STATE";
    }
    else if(state == ZOO_CONNECTING_STATE){
        return "ZOO_CONNECTING_STATE";
    }
    else if(state == ZOO_ASSOCIATING_STATE){
        return "ZOO_ASSOCIATING_STATE";
    }
    else if(state == ZOO_CONNECTED_STATE){
        return "ZOO_CONNECTED_STATE";
    }
    else if(state == ZOO_READONLY_STATE){
        return "ZOO_READONLY_STATE";
    }
    else if(state == ZOO_NOTCONNECTED_STATE){
        return "ZOO_NOTCONNECTED_STATE";
    }
    else{
        return "UNKNOWN_ZOO_STATE";
    }
}
} // namespace inner

enum class EN_ZK_MODE
{
    SLAVE = 0,
    MASTER = 1
};

class CZkWrapper final
{
    using OnZkElectionCB = eddid_function_wrap<void(EN_ZK_MODE)>;
public:
    CZkWrapper() = delete;
    /**
     * \param base libevent base handle.
     * \param th class this pointer.
     * \param host comma separated host:port pairs, each corresponding to a zk server. 
     *   e.g. "127.0.0.1:2181,127.0.0.1:2182,127.0.0.1:2183"
     */
    CZkWrapper(event_base* base, const std::string& host):
        m_base(base),
        m_host(host),
        m_zkHle(nullptr),
        m_createdNodeCount(0),
        m_isCreatedAllNode(false),
        m_zkFd(-1){}

    ~CZkWrapper()
    {
        //event base should be free outside
        StopWatch();
    }
    /**
     * \param cb callback function when election occurs.
     * \param nodePath create Znode's full path. 
     *   e.g. "/eddid_feed/node001"
     * \return 0 success -1 failure
     */
    int StartWatch(const OnZkElectionCB& cb, const std::string& nodePath)
    {  
        //param check
        if(nodePath.empty() || nodePath[0] != '/' || nodePath[nodePath.length() - 1] == '/'){
            std::cerr << "Error nodepath format" << std::endl;
            return -1;
        }

        m_zkElectCb = cb;
        m_recvTimeoutMs = 2000;
        int pos = nodePath.find_last_of('/');
        m_dirPath = nodePath.substr(0, pos);
        m_nodeName = nodePath.substr(pos + 1);
        int start = 1;//skip the first '/'
        for (int pos = 1; pos < nodePath.size(); ++pos)
        {
            if (nodePath[pos] == '/'){
                m_nodeNameVec.emplace_back(nodePath.substr(start, pos - start));
                start = pos + 1;
            }
        }
        m_nodeNameVec.emplace_back(nodePath.substr(start));
        // init signal event, if you use signal() func, libevent can't catch any signal!
        event_set(&m_signalintEvent, SIGINT, EV_PERSIST | EV_SIGNAL, OnSignalintEvent, (void*)this);
        event_base_set(m_base, &m_signalintEvent);
        event_add(&m_signalintEvent, nullptr);
        
        event_set(&m_signaltermEvent, SIGTERM, EV_PERSIST | EV_SIGNAL, OnSignaltermEvent, (void*)this);
        event_base_set(m_base, &m_signaltermEvent);
        event_add(&m_signaltermEvent, nullptr);
        // start poll
        PollZKInterest();
        return 0;
    }
    /**
     * \return  0 success, others failure
     */
    int StopWatch()
    {
        // stop reconnect timmer
        event_del(&m_interestEvent);
        return DoStop();
    }

private:
    static void OnSignalintEvent(evutil_socket_t fd, short event, void* args)
    {
        if(args == nullptr){
            std::cerr << "OnSignalEvent() failed, args is nullptr" << std::endl;
            return;
        }
        CZkWrapper* pThis = static_cast<CZkWrapper*>(args);
        pThis->DoSignal(SIGINT);
    }

    static void OnSignaltermEvent(evutil_socket_t fd, short event, void* args)
    {
        if(args == nullptr){
            std::cerr << "OnSignalEvent() failed, args is nullptr" << std::endl;
            return;
        }
        CZkWrapper* pThis = static_cast<CZkWrapper*>(args);
        pThis->DoSignal(SIGTERM);
    }

    static void OnInterestTimer(evutil_socket_t fd, short event, void* args)
    {
        if(args == nullptr){
            std::cerr << "OnInterestTimer() failed, args is nullptr"<<std::endl;
            return;
        }
        CZkWrapper* pThis = static_cast<CZkWrapper*>(args);
        pThis->PollZKInterest();
    }

    static void OnZKWatch(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
    {
        std::cout << "OnZKWatch(): type = " << type << " <" << inner::type2string(type) << ">"
            << ", state = " << state << " <" << inner::state2string(state)  << ">" 
            << ", path = " << path <<std::endl;
        if(watcherCtx == nullptr){
            std::cerr << "OnZKWatch() failed, watcherCtx is null" <<std::endl;
            return; 
        }
        CZkWrapper* pThis = static_cast<CZkWrapper*>(watcherCtx);
        if (type == ZOO_SESSION_EVENT){
            if (state == ZOO_CONNECTED_STATE){
                std::cout << "Connected ZK server, child_id = " << zoo_client_id(zh) << std::endl;
                // real connected zk server, start to create Znode
                pThis->CreateZNode(0);
            }
            // TODO: do reconnect
            else if (state == ZOO_CONNECTING_STATE){
            }
            else if (state == ZOO_EXPIRED_SESSION_STATE){
            }
            else if (state == ZOO_AUTH_FAILED_STATE){
            }
            else if (state == ZOO_NOTCONNECTED_STATE){
            }
        }
        else if (type == ZOO_CREATED_EVENT){
        }
        else if (type == ZOO_DELETED_EVENT){
        }
        else if (type == ZOO_CHANGED_EVENT){
        }
        else if (type == ZOO_CHILD_EVENT){
            pThis->GetChildrenList();
        }
        else{
            printf("OnZKWatch(): unknown type = %d\n", type);
        }
    }

    static void OnEventIOCb(evutil_socket_t fd, short event, void* args)
    {
        if(args == nullptr){
            std::cerr << "OnEventIOCb() failed, args is nullptr"<<std::endl;
            return;
        }
        CZkWrapper* pThis = static_cast<CZkWrapper*>(args);
        int zkEvent = ((event & EV_READ) ? ZOOKEEPER_READ : 0) | ((event & EV_WRITE) ? ZOOKEEPER_WRITE : 0);
        int ret = zookeeper_process(pThis->m_zkHle, zkEvent);
        if (ret != ZOO_ERRORS::ZOK) {
            // not an error
            if (ret != ZOO_ERRORS::ZNOTHING) {
                int sysErrorNo = ((ret == ZOO_ERRORS::ZSYSTEMERROR) ? errno : 0);
                std::cerr << "zookeeper_process() failed, ret = " << ret << " <" << zerror(ret) << ">" 
                    << ", errno = " << sysErrorNo << " <" << std::strerror(sysErrorNo) << ">" << std::endl;
                pThis->DoStop();
                return;
            }
        }
    }

    static void OnCreateZNodeCb(int rc, const char *value, const void *data)
    {
        if(data == nullptr){
            std::cerr << "OnCreateZNodeCb() failed, data is nullptr"<<std::endl;
            return;
        }
        CZkWrapper* pThis = static_cast<CZkWrapper*>(const_cast<void*>(data));
        pThis->CheckZNodeCreate(rc, value);
    }

    static void OnGetChildrenCb(int rc, const struct String_vector *childs, const void *data)
    {
        if(data == nullptr){
            std::cerr << "OnGetChildrenCb() failed, data is nullptr"<<std::endl;
            return;
        }
        if(childs == nullptr){
            std::cerr << "OnGetChildrenCb() failed, childs is nullptr"<<std::endl;
            return;
        }
        CZkWrapper* pThis = static_cast<CZkWrapper*>(const_cast<void*>(data));
        pThis->ElectMaster(rc, childs);
        deallocate_String_vector(const_cast<struct String_vector*>(childs));
    }

private:
    void PollZKInterest()
    {
        int ret = InitZK();
        if(ret != 0){
            return;
        }
        int interest = 0;
        timeval tv = { 0, 0 };
        // get zk fd and interest event
        ret = zookeeper_interest(m_zkHle, &m_zkFd, &interest, &tv);
        if(ret != ZOO_ERRORS::ZOK || m_zkFd == -1){
            std::cerr << "zookeeper_interest() failed, ret = " << ret << " <" << zerror(ret) << ">" <<", fd = " << m_zkFd <<std::endl;
            DoStop();
            return;
        }
        // tv is next time to call zookeeper_interest()
        event_del(&m_interestEvent);
        event_set(&m_interestEvent, -2, EV_PERSIST, OnInterestTimer, (void*)this);
        event_base_set(m_base, &m_interestEvent);
        event_add(&m_interestEvent, &tv);

        // use libevent to monitor zk interested fd and event
        int events = (interest & ZOOKEEPER_READ ? EV_READ : 0) | (interest & ZOOKEEPER_WRITE ? EV_WRITE : 0);
        event_del(&m_fdEvent);
        event_set(&m_fdEvent, m_zkFd, events | EV_PERSIST, OnEventIOCb, (void*)this);
        event_base_set(m_base, &m_fdEvent);
        event_add(&m_fdEvent, nullptr);
    }

    int InitZK()
    {
        if (m_zkHle == nullptr){
            //m_recvTimeoutMs is session timeout, but zk server usually not use this timeout
            m_zkHle = zookeeper_init(m_host.c_str(), OnZKWatch, m_recvTimeoutMs, 0, (void *)this, 0);
            if (m_zkHle == nullptr){
                std::cerr << "Create zk handle failed, error msg: " << std::strerror(errno) << std::endl;
                return -1;
            }
        }
        return 0;
    }

    int CreateZNode(int dirLevel)
    {
        if(dirLevel < 0 || dirLevel >= m_nodeNameVec.size()){
            std::cerr << "Create znode failed, dirLevel = " << dirLevel << ", m_dirVec size = " << m_nodeNameVec.size() <<std::endl;
            return -1;
        }
        m_createdNodeCount = dirLevel + 1;
        std::string parentPath = "/" + m_nodeNameVec[0];
        for(int i = 1; i < m_createdNodeCount; ++i)
        {
            parentPath += ("/" + m_nodeNameVec[i]);
        }
        std::string nodeData = parentPath;
        int flag = ((m_createdNodeCount < m_nodeNameVec.size()) ? 0 : ZOO_EPHEMERAL);

        int ret = zoo_acreate(m_zkHle, parentPath.c_str(), nodeData.c_str(), nodeData.size(), &ZOO_OPEN_ACL_UNSAFE, flag,
                OnCreateZNodeCb, (void*)this);
        if (ret != ZOO_ERRORS::ZOK) {
            std::cerr << "zoo_acreate() failed, ret = " << ret << " <" << zerror(ret) << ">" <<std::endl;
            return -1;
        }
        return 0;
    }

    void CheckZNodeCreate(int rc, const char *value)
    {
        if (rc == ZOO_ERRORS::ZOK) {
            std::cout<<"Create Znode success, node path = " << value <<std::endl;
            if (m_createdNodeCount < m_nodeNameVec.size()) {
                // not last node, create next
                CreateZNode(m_createdNodeCount);
            } 
            else {
                // has create all node, get children list
                m_isCreatedAllNode = true;
                GetChildrenList();
            }
        } 
        else if (rc == ZNODEEXISTS) {
            if (m_createdNodeCount < m_nodeNameVec.size()) {
                // node has exist, create next
                CreateZNode(m_createdNodeCount);
            } 
            else {
                // last node has exist
                if (m_isCreatedAllNode) {
                    // maybe reconnect happen, just get children list
                    GetChildrenList();
                } 
                else{
                    std::cerr << "Node has exist, node path" << m_dirPath + "/" + m_nodeName <<std::endl;
                }
            }
        }
        else if (rc == ZNONODE) {
            // parent node may be not exit, create it
            if (m_createdNodeCount >= 2) {
                CreateZNode(m_createdNodeCount - 2);
            }
        } 
        else {
            std::cerr << "Ceeate node failed, rc = " << rc << " <" << zerror(rc) << ">" << std::endl;
        }
    }

    int GetChildrenList()
    {
        const int isNotify = 1; //if node change, need notify
        int ret = zoo_aget_children(m_zkHle, m_dirPath.c_str(), isNotify, OnGetChildrenCb, (void*)this);
        if (ret != ZOO_ERRORS::ZOK) {
            std::cerr << "Get children list failed, ret = " << ret << " <" << zerror(ret) << ">" <<std::endl;
        }
        return ret;
    }

    void ElectMaster(int rc, const struct String_vector *childs)
    {
        if (rc == ZOO_ERRORS::ZOK) {
            // use first child as master
            // TODO: no need to cycle, just compare m_nodeName with childs->data[0]
            int matchIdx = -1;
            for (int i = 0; i < childs->count; ++i) {
                if (m_nodeName == childs->data[i]) {
                    matchIdx = i;
                    break;
                }
            }

            std::cout<< "Total " << childs->count << " children: " <<std::endl;
            for (int i = 0; i < childs->count; ++i) {
                std::cout<< "child num: " << i << ", name: " << childs->data[i] <<std::endl;
            }

            if (matchIdx < 0) {
                // not find this node, create it again
                CreateZNode(m_nodeNameVec.size() - 1);
            } 
            else {
                // callback
                EN_ZK_MODE mode = ((matchIdx == 0) ? EN_ZK_MODE::MASTER : EN_ZK_MODE::SLAVE);
                if (m_zkElectCb) {
                    m_zkElectCb(mode);
                }
                std::cout<< "==> change to <" << (mode == EN_ZK_MODE::MASTER ? "Master" : "Slave") << ">" << std::endl;
            }
        } 
        else {
            std::cerr << "Get children list failed, rc = " << rc << " <" << zerror(rc) << ">" <<std::endl;
        }
    }

    void DoSignal(int signal)
    {
        std::cout << "Catch signal: " << signal << std::endl;
		StopWatch();
    }

    int DoStop()
    {
        int ret = 0;
        // stop event
        event_del(&m_signalintEvent);
        event_del(&m_signaltermEvent);
        event_del(&m_fdEvent);
        // free zk handle
        if(m_zkHle){
            ret  = zookeeper_close(m_zkHle);
            std::cout << "zookeeper_close() ret = " << ret << " <" << zerror(ret) << ">" <<std::endl;
            m_zkHle = NULL;
            m_zkFd = -1;
        }
        return ret;
    }

private:
    event_base* m_base;
    std::string m_host;
    OnZkElectionCB m_zkElectCb;
    int m_recvTimeoutMs;
    std::string m_dirPath;
    std::string m_nodeName;
    std::vector<std::string> m_nodeNameVec;
    int m_createdNodeCount;
    bool m_isCreatedAllNode;
    event m_interestEvent;
    zhandle_t* m_zkHle;
    int m_zkFd;
    event m_fdEvent;
    event m_signalintEvent;
    event m_signaltermEvent;
};


} // namespace zkutils
} // namespace eddid

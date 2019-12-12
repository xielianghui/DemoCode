#include "lws_client.h"

static struct lws_context_creation_info ctxInfo = {0};
static struct lws_context* pCtx = nullptr;
static struct lws_client_connect_info conInfo = {0};
static struct lws* pWsi = nullptr;
static struct lws_protocols protocols = {0};

static int LwsClientCb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    struct session_data *data = (struct session_data *) user;
    LwsClient* lct = (LwsClient*)lws_context_user(lws_get_context(wsi));
    switch (reason)
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:   // 连接到服务器后的回调
            lct->getCbPtr()->OnConnect();
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:       // 接收到服务器数据后的回调，数据为in，其长度为len
        {
            std::string msgStr((char *) in, len);
            lct->getCbPtr()->OnRecv(std::move(msgStr));
            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE:     // 当此客户端可以发送数据时的回调
        {
            std::string reqStr = lct->getOneReq();
            if(reqStr.empty()) break;
            // 前面LWS_PRE个字节必须留给LWS
            memset(data->buf, 0, sizeof(data->buf));
            char *msg = (char *) &data->buf[LWS_PRE];
            int len = snprintf(msg, MAX_PAYLOAD_SIZE, "%s", reqStr.c_str());
            len = (len > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : len);
            // 通过WebSocket发送文本消息
            lws_write(pWsi, &data->buf[LWS_PRE], len, LWS_WRITE_TEXT);
            break;
        }
        default:
            break;
    }
    return 0;
}

LwsClient::LwsClient(event_base* pEvBase):
    m_port(8500),
    m_pCb(nullptr),
    m_pEvBase(pEvBase),// will free by ev_loop.cpp
    m_lpTimerEv(nullptr){}

LwsClient::~LwsClient()
{
    if(m_lpTimerEv){
        event_free(m_lpTimerEv);
    }
    lws_context_destroy(pCtx);
}

void LwsClient::Register(LwsCbInterface* pCb)
{
    m_pCb = pCb;
}

int LwsClient::Connect(std::string addr, int port)
{
    m_addr = addr;
    m_port = port;
    // protocols set
    protocols.name  = "ws";
    protocols.callback = &LwsClientCb;
    protocols.per_session_data_size = sizeof(struct session_data);
    protocols.rx_buffer_size = 0;
    protocols.id = 0;
    protocols.user = NULL;
    protocols.tx_packet_size = 0;
    // ctx set
    ctxInfo.port = CONTEXT_PORT_NO_LISTEN;
    ctxInfo.iface = NULL;
    ctxInfo.protocols = &protocols;
    ctxInfo.gid = -1;
    ctxInfo.uid = -1;
    ctxInfo.user = this;
    // ssl set
    ctxInfo.ssl_ca_filepath = NULL;
    ctxInfo.ssl_cert_filepath = NULL;
    ctxInfo.ssl_private_key_filepath = NULL;
    // create context
    pCtx = lws_create_context(&ctxInfo);
    if(!pCtx){
        printf("Create ctx failed\n");
        return -1;
    }
    // connect info set
    std::string host = (m_addr + ":" + std::to_string(m_port));
    conInfo.context = pCtx;
    conInfo.address = m_addr.c_str();
    conInfo.port = m_port;
    conInfo.ssl_connection = 0;
    conInfo.path = "./";
    conInfo.host = host.c_str();
    conInfo.origin = host.c_str();
    conInfo.protocol = protocols.name;
    // create connect
    pWsi = lws_client_connect_via_info(&conInfo);
    if(!pWsi){
        printf("Create ctx failed\n");
        return -1;
    }
    // run timer
    if(!m_pEvBase){
        puts("Empty event_base pointer\n");
        return -1;
    }
    m_lpTimerEv = event_new(m_pEvBase, -1, EV_PERSIST , OnRunTimer, this);
    if (m_lpTimerEv == nullptr) {
        puts("event_new() run timer failed\n");
        return -1;
    }
    timeval tv;
    evutil_timerclear(&tv);
    tv.tv_usec = 100000;// 100ms
    event_add(m_lpTimerEv, &tv);
    return 0;
}

int LwsClient::SendReq(const std::string& msg)
{
    m_reqQue.push(msg);
    return 0;
}

void LwsClient::Run()
{
    if(!pWsi){
        pWsi = lws_client_connect_via_info(&conInfo);
    }
    int times = 1000;
    while(times > 0)
    {
        lws_service(pCtx, 250);
        lws_callback_on_writable(pWsi);
        --times;
    }
}

LwsCbInterface* LwsClient::getCbPtr()
{
    return m_pCb;
}

std::string LwsClient::getOneReq()
{
    if(m_reqQue.empty()){
        return "";
    }
    std::string reqStr = m_reqQue.front();
    m_reqQue.pop();
    return reqStr;
}

void LwsClient::OnRunTimer(evutil_socket_t fd, short event, void* args)
{
    LwsClient* lc = (LwsClient*) args;
    lc->Run();
}
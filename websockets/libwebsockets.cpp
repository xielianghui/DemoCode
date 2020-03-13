#include <iostream>
#include <string>


#include "libwebsockets.h"

#define MAX_PAYLOAD_SIZE  10 * 1024

static struct lws_context_creation_info ctx_info = { 0 };
static struct lws_context *context = NULL;
static struct lws_client_connect_info conn_info = { 0 };
static struct lws *wsi = NULL;

/**
 * 会话上下文对象，结构根据需要自定义
 */
struct session_data {
    unsigned char buf[LWS_PRE + MAX_PAYLOAD_SIZE];
};

static int times = 0;
/**
 * 某个协议下的连接发生事件时，执行的回调函数
 * wsi：指向WebSocket实例的指针
 * reason：导致回调的事件
 * user 库为每个WebSocket会话分配的内存空间
 * in 某些事件使用此参数，作为传入数据的指针
 * len 某些事件使用此参数，说明传入数据的长度
 */
int lws_client_callback( struct lws *wsi,
                         enum lws_callback_reasons reason,
                         void *user,
                         void *in,
                         size_t len )
{
    struct session_data *data = (struct session_data *) user;
    switch (reason)
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:   // 连接到服务器后的回调
            std::cout<<"Connected to server ok"<<std::endl;
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:       // 接收到服务器数据后的回调，数据为in，其长度为len
            printf("Recv: %s\n", (char *)in);
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:     // 当此客户端可以发送数据时的回调
        {
            if(times >= 1) break;
            ++times;
            std::cout << "Can send data now" << std::endl;
            // 前面LWS_PRE个字节必须留给LWS
            memset(data->buf, 0, sizeof(data->buf));
            char *msg = (char *) &data->buf[LWS_PRE];
            std::string reqStr = "{\"reqtype\":1,\"reqid\":1,\"session\":\"\",\"data\":{\"connectionid\":1}}";
            int len = sprintf(msg, "%s", reqStr.c_str());
            printf("Send: %s\n", msg);
            // 通过WebSocket发送文本消息
            lws_write(wsi, &data->buf[LWS_PRE], len, LWS_WRITE_TEXT);
            break;
        }
        default:
            printf("case:%d\n", reason);
    }
    return 0;
}

static struct lws_protocols protocols;


int main(int args, char** argv)
{
    const char* serverAddr = "119.23.15.70";
    int port = 8500;

    // protocols set
    protocols.name  = "ws";
    protocols.callback = &lws_client_callback;
    protocols.per_session_data_size = sizeof(struct session_data);
    protocols.rx_buffer_size = 0;
    protocols.id = 0;
    protocols.user = NULL;
    protocols.tx_packet_size = 0;
    // ctx set
    ctx_info.port = CONTEXT_PORT_NO_LISTEN;
    ctx_info.iface = NULL;
    ctx_info.protocols = &protocols;
    ctx_info.gid = -1;
    ctx_info.uid = -1;

    // ssl
    ctx_info.ssl_ca_filepath = NULL;
    ctx_info.ssl_cert_filepath = NULL;
    ctx_info.ssl_private_key_filepath = NULL;

    // 创建一个WebSocket处理器
    context = lws_create_context( &ctx_info );
    if(!context){
        std::cout<<"Create ctx failed"<<std::endl;
        return -1;
    }

    char addrPort[256] = { 0 };
    sprintf(addrPort, "%s:%u", serverAddr, port & 65535);
    std::cout<<"Connect to: "<<addrPort<<std::endl;
    // 客户端连接参数
    conn_info = { 0 };
    conn_info.context = context;
    conn_info.address = serverAddr;
    conn_info.port = port;
    conn_info.ssl_connection = 0;
    conn_info.path = "./";
    conn_info.host = addrPort;
    conn_info.origin = addrPort;
    conn_info.protocol = protocols.name;

    // 创建一个客户端连接
    wsi = lws_client_connect_via_info(&conn_info);
    if(!wsi){
        std::cout<<"Create ctx failed"<<std::endl;
        return -1;
    }
    int waitTime = 1000;
    while(true)
    {
        lws_service(context, waitTime);
        lws_callback_on_writable(wsi);
    }

    //销毁资源
    lws_context_destroy(context);

    return 0;
}






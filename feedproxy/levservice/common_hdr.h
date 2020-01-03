#pragma once
#include "common_cmd.h"

#define START_HDR   0xEF
#define END_HDR     0xCD
#define PROTO_START 0xFD
#define PROTO_END   0xFC
#define HEART_DATA_LEN 64

// 通用通信协议头
namespace ngp {
    
    constexpr uint16_t COMMON_HDR_LEN = 16;
    
#pragma pack(push)
#pragma pack(1)

typedef struct __tag_ST_COMMONHDR
{
    uint8_t start_;
    uint16_t cmd_;            // 包头
    uint32_t data_len_;       // 包体数据长
    uint64_t async_num_;      // 异步包编号 
    uint8_t  end_;
    __tag_ST_COMMONHDR()
    {
        memset(this, 0, sizeof(*this));
        start_ = START_HDR;
        end_   = END_HDR;
    }
    
    __tag_ST_COMMONHDR(uint16_t cmd, uint32_t len)
    {
        memset(this, 0, sizeof(*this));
        start_ = START_HDR;
        end_   = END_HDR;
        cmd_   = cmd;
        data_len_ = len;
    }
    
    __tag_ST_COMMONHDR(uint16_t cmd, uint32_t len, uint64_t async_num)
    {
        memset(this, 0, sizeof(*this));
        start_ = START_HDR;
        end_   = END_HDR;
        cmd_   = cmd;
        async_num_ = async_num;
        data_len_ = len;
    }
    
}ST_COMMONHDR, *LPST_COMMONHDR;

// 协议调用数据结构 注一期完工后，将使用此结构压缩数据
typedef struct __tag_ST_PROTO_DATA
{
    uint8_t start_;
    uint8_t map_len_;          // 数据位图数据长
    char*   data_;             // pu实际要发送的数据，由位图和数据组成
    uint8_t end_;
    __tag_ST_PROTO_DATA()
    {
        memset(this, 0, sizeof(*this));
        start_ = PROTO_START;
        end_ = PROTO_END;
    }
}ST_PROTO_DATA, *LPST_PROTO_DATA;


typedef struct __tag_ST_HERAT_SYNC
{
    char auth_info_[HEART_DATA_LEN];            // 心跳中携带登陆时取到的认证串，如果没有，则为非法的连接，将关闭此连接
                                                // 内网之间，此字段为空
    __tag_ST_HERAT_SYNC()
    {
        memset(this, 0, sizeof(*this));
    }
}ST_HERAT_SYNC, *LPST_HERAT_SYNC;


#pragma pack(pop)
} // end of namespace ngp

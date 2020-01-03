#pragma once

#include <inttypes.h>

#define START_HDR   0xEF
#define END_HDR     0xCD
#define PROTO_START 0xFD
#define PROTO_END   0xFC
#define HEART_DATA_LEN 64

// 通用通信协议头
namespace eddid {
    
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
#pragma pack(pop)
} // end of namespace eddid

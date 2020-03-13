#pragma once
#include <arpa/inet.h>
#include <stdlib.h>
#include <time.h>
#include <endian.h>

#include "../utils/eddid_common.h"
#include "../utils/eddid_functional.h"

namespace eddid{
namespace wsutils{

static uint64_t ntohll(uint64_t val)
{
    if (__BYTE_ORDER == __LITTLE_ENDIAN){
        return (((uint64_t)htonl((int)((val << 32) >> 32))) << 32) | (uint32_t)htonl((int)(val >> 32));
    }
    else if (__BYTE_ORDER == __BIG_ENDIAN){
        return val;
    }
    else{
        return 0;
    }
}

static uint64_t htonll(uint64_t val)
{
    if (__BYTE_ORDER == __LITTLE_ENDIAN){
        return (((uint64_t)htonl((int)((val << 32) >> 32))) << 32) | (uint32_t)htonl((int)(val >> 32));
    }
    else if (__BYTE_ORDER == __BIG_ENDIAN){
        return val;
    }
    else{
        return 0;
    }
}

static uint32_t defaultDataMaxSize = 1024 * 1024;
static const uint32_t payload2BytesLen = 126;
static const uint32_t payload8BytesLen = 127;
static const uint32_t twoBytesMax = 65535; // 2^16 = 65536

enum EN_OP_CODE: uint8_t 
{
    FRAME = 0,
    TEXT = 1,
    BINARY = 2,
    CLOSE = 8,
    PING = 9,
    PONG = 10
};

#pragma pack(push,1)
struct ST_WebScoketHead // little end
{
    uint8_t opCode : 4;
    uint8_t rsv : 3;
    uint8_t fin : 1;
    uint8_t payloadLen : 7;
    uint8_t mask : 1;
};
#pragma pack(pop)


class WebSocketProto
{
    using OnData = eddid_function_wrap<void(std::shared_ptr<char>&&, uint32_t, EN_OP_CODE)>;

public:
    WebSocketProto():m_headSize(sizeof(ST_WebScoketHead))
    {
        m_pkgCache.reserve(2 * defaultDataMaxSize);
        srand((uint32_t)time(NULL));
    }
    ~WebSocketProto() = default;

public:

	/**
	 * \brief 解析WebSocket头协议.
     * \param rawMsg 原始包含WebSocket头的数据
     * \param len 原始数据的长度
	 * \return 0 成功解析,并且调用回调函数, -1 数据长度不够,等待下一条数据合并, -2 发生错误,需要断开WebSocket连接
	 */
    int ParseMsg(std::shared_ptr<char>&& rawMsg, uint32_t len)
    {
        m_pkgCache += std::string(rawMsg.get(), len);
        while(true)
        {
            if(m_pkgCache.size() < m_headSize) return CloseClear();// 控制帧不可能被分割,异常状态断开连接
            // get data head
            ST_WebScoketHead* pHead = (ST_WebScoketHead*)m_pkgCache.data();
            EN_OP_CODE opCode = (EN_OP_CODE)pHead->opCode;
            if(!CheckOpCode(opCode)) return CloseClear(); // 断开连接操作码或者未知操作码,断开连接

            uint64_t dataLen = 0; // 真实数据长度
            uint32_t keyLen = 0;  // 表示长度使用的字节数
            uint32_t maskLen = (pHead->mask ? 4 : 0); // 如果有设置掩码标志,掩码长度为4字节
            if(pHead->payloadLen < payload2BytesLen){// payloadLen小于125,则它即为数据长度
                dataLen = pHead->payloadLen;
            }
            else if(pHead->payloadLen == payload2BytesLen){// payloadLen等于126, 后面2字节用来表示数据长度
                keyLen = 2;
                if(m_pkgCache.size() < (keyLen + m_headSize)) return -1;
                uint16_t l = ntohs(*(uint16_t*)(m_pkgCache.data() + m_headSize));
                dataLen = l;
            }
            else{// payloadLen等于127, 后面字节用来表示数据长度
                keyLen = 8;
                if(m_pkgCache.size() < (keyLen + m_headSize)) return -1;
                uint64_t l = ntohll(*(uint64_t*)(m_pkgCache.data() + m_headSize));
                dataLen = l;
            }
            if(dataLen > defaultDataMaxSize) return CloseClear(); // 数据包长度大于阈值,断开连接
            uint32_t totalHeadLen = m_headSize + keyLen + maskLen;
            if(m_pkgCache.size() < (totalHeadLen + dataLen)) return -1;

            char* dataStart = (char*)(m_pkgCache.data() + totalHeadLen);
            if(pHead->mask){// 需要掩码解码
                const char* cMask = m_pkgCache.data() + m_headSize + keyLen;
                for(int i = 0; i < dataLen; ++i)
                {
                    dataStart[i] ^= cMask[i % maskLen];
                }
            }
            m_dataCache += std::string(dataStart, dataLen);
            if(pHead->fin){// indicates that this is the final fragment in a message
                std::shared_ptr<char> realData(new char[m_dataCache.size()]{0}, std::default_delete<char[]>());
                memcpy(realData.get(), m_dataCache.data(), m_dataCache.size());
                m_cb(std::move(realData), m_dataCache.size(), opCode);
                m_dataCache.clear();
                return 0;
            }
            m_pkgCache.erase(0, totalHeadLen + dataLen);
            if(m_pkgCache.empty()) return 0;
        }

    }

    /**
     * \brief 添加WebSocket头协议.
     * \param opCode 数据类型,详见EN_OP_CODE.
     * \param rawMsg 原始数据.
     * \param len 原始数据长度.
     * \param dataMaxSize 每个包数据的最大大小,不填使用默认值.
     * \return 返回打包后的结果,如果数据的大小大于最大限制就会分包并且按照顺序放入返回的vector中.
     */
    template<bool isServer>
    std::vector<std::pair<std::shared_ptr<char>, uint32_t>> PackMsg(const EN_OP_CODE& opCode, 
                                                                    std::shared_ptr<char>&& rawMsg, 
                                                                    uint32_t len, 
                                                                    uint32_t dataMaxSize = defaultDataMaxSize)
    {
        int pos = 0; // rawMsg的偏移量
        bool isFirstPkgs = true;
        std::vector<std::pair<std::shared_ptr<char>, uint32_t>> res;
        while(true)
        {
            char* data = (rawMsg.get() + pos);
            m_sendHead.fin = (len <= dataMaxSize ? 1 : 0);
            m_sendHead.rsv = 0;
            m_sendHead.opCode = (isFirstPkgs ? (uint8_t)opCode : (uint8_t)EN_OP_CODE::FRAME);// 发生分包的情况下,不是第一个包的opcode都为0
            m_sendHead.mask = (!isServer ? 1 : 0);
            
            uint32_t keyLen = 0;// 表示长度使用的字节数
            uint32_t maskLen = (m_sendHead.mask ? 4 : 0); // 掩码字节数
            uint32_t dataLen = (len < dataMaxSize ? len : dataMaxSize); // 数据长度 
            if(dataLen < payload2BytesLen){
                m_sendHead.payloadLen = dataLen;
            }
            else if(dataLen < twoBytesMax){
                keyLen = 2;
                m_sendHead.payloadLen = payload2BytesLen;
            }
            else{
                keyLen = 8;
                m_sendHead.payloadLen = payload8BytesLen;
            }

            uint32_t totalLen = m_headSize + keyLen + maskLen + dataLen;// 整个包的长度
            std::shared_ptr<char> pkg; // 整个包的指针
            pkg.reset(new char[totalLen]{0}, std::default_delete<char[]>());
            // start to write
            char* start = pkg.get();
            memcpy(start, &m_sendHead, m_headSize);
            if(keyLen == 2){
                uint16_t dl = htons(dataLen);
                memcpy(start + m_headSize, &dl, sizeof(uint16_t));
            }
            else if(keyLen == 8){
                uint64_t dl = htonll(dataLen);
                memcpy(start + m_headSize, &dl, sizeof(uint64_t));
            }
            if(m_sendHead.mask){
                char cMask[maskLen]{0};
                for(int i = 0; i < maskLen; ++i)
                {
                    cMask[i] = rand() % 255;
                }
                memcpy(start + m_headSize + keyLen, cMask, maskLen);
                for(int i = 0; i < dataLen; ++i)
                {
                    data[i] ^= cMask[i % maskLen];
                }
            }
            memcpy(start + m_headSize + keyLen + maskLen, data, dataLen);
            res.emplace_back(pkg, totalLen);
            pos += dataLen;
            len -= dataLen;
            if(len <= 0) break;
            isFirstPkgs = false;
        }
        return res;
    }

    void RegisterCallback(const OnData& cb)
    {
        m_cb = cb;
    }

    void SetDefaultDataMaxSize(const int32_t& size)
    {
        defaultDataMaxSize = size;
    }

private:
    int CloseClear()
    {
        m_pkgCache.clear();
        m_dataCache.clear();
        return -2;
    }

    bool CheckOpCode(const EN_OP_CODE& opCode)
    {
        if((opCode == EN_OP_CODE::CLOSE) || 
           (!(opCode == EN_OP_CODE::FRAME || opCode == EN_OP_CODE::TEXT ||
              opCode == EN_OP_CODE::BINARY || opCode == EN_OP_CODE::PING || 
              opCode == EN_OP_CODE::PONG))
        )
        {
            return false;
        }
        return true;
    }

private:
    OnData m_cb;
    uint32_t m_headSize;
    std::string m_pkgCache;
    std::string m_dataCache;
    ST_WebScoketHead m_sendHead;
};

}
}
#pragma once
#include <string>

namespace eddid
{

enum EN_OP_CODE: unsigned char {
    FRAME = 0,
    TEXT = 1,
    BINARY = 2,
    CLOSE = 8,
    PING = 9,
    PONG = 10
};

static bool ParseURI(std::string &uri, bool &secure, std::string &hostname, int &port, std::string &path)
{
    port = 80;
    secure = false;
    size_t offset = 5;
    if (!uri.compare(0, 6, "wss://")){
        port = 443;
        secure = true;
        offset = 6;
    }
    else if (uri.compare(0, 5, "ws://")){
        return false;
    }

    if (offset == uri.length()){
        return false;
    }

    if (uri[offset] == '['){
        if (++offset == uri.length())
        {
            return false;
        }
        size_t endBracket = uri.find(']', offset);
        if (endBracket == std::string::npos)
        {
            return false;
        }
        hostname = uri.substr(offset, endBracket - offset);
        offset = endBracket + 1;
    }
    else{
        hostname = uri.substr(offset, uri.find_first_of(":/", offset) - offset);
        offset += hostname.length();
    }

    if (offset == uri.length()){
        path.clear();
        return true;
    }

    if (uri[offset] == ':'){
        offset++;
        std::string portStr = uri.substr(offset, uri.find('/', offset) - offset);
        if (portStr.length()){
            try{
                port = stoi(portStr);
            }
            catch (...){
                return false;
            }
        }
        else{
            return false;
        }
        offset += portStr.length();
    }

    if (offset == uri.length()){
        path.clear();
        return true;
    }

    if (uri[offset] == '/'){
        path = uri.substr(++offset);
    }
    return true;
}

#if 0
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
#endif


}

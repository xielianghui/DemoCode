#pragma once

class ConvertUtils
{
public:
    static void ProtoReq2JsonReq(int64_t reqid, const std::string& protoReq, std::string& jsonReq)
    {
        //jsonReq = "{\"reqtype\":1,\"reqid\":1,\"session\":\"\",\"data\":{\"connectionid\":1}}";
        jsonReq = protoReq;
    }

    static void JsonRes2ProtoRes(int64_t& reqid, const std::string& jsonRes, std::string& protoRes)
    {
        protoRes = jsonRes;
    }
};
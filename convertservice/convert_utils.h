#pragma once

#include "json/json.h"

class ConvertUtils
{
public:
    static int ProtoReq2JsonReq(int64_t reqid, const std::string& reqProtoStr, Json::Value& reqJson)
    {
        reqJson.clear();
        // for test
        Json::Reader jsonReader;
        jsonReader.parse(reqProtoStr, reqJson);
        reqJson["reqid"] = reqid;
        // --------

        return 0;
    }

    static int JsonRes2ProtoRes(int64_t& reqid, const Json::Value& resJson, std::string& resProto)
    {
        reqid = resJson.isMember("reqid") && resJson["reqid"].isInt() ? resJson["reqid"].asInt() : -1;
        if(reqid == -1){
            printf("Get req id failed, msg: %s\n", resJson.toStyledString().c_str());
            return -1;
        }
        // convert
        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        resProto = Json::writeString(builder, resJson);
        return 0;
    }
};